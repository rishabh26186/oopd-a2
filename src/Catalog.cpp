#include "bookmgmt/Catalog.h"

#include "bookmgmt/Book.h"
#include "bookmgmt/Journal.h"
#include "bookmgmt/EBook.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

namespace {
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return s;
}
}  // namespace

Resource& Catalog::add(std::unique_ptr<Resource> r) {
    if (!r) throw std::invalid_argument("cannot add a null resource");
    const std::string id = r->id();
    if (items_.count(id)) throw DuplicateIdError(id);
    Entry& e = items_[id];
    e.resource = std::move(r);
    return *e.resource;
}

bool Catalog::contains(const std::string& id) const { return items_.count(id) > 0; }

Resource* Catalog::find(const std::string& id) {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.resource.get();
}

const Resource* Catalog::find(const std::string& id) const {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.resource.get();
}

Resource& Catalog::get(const std::string& id) {
    if (Resource* r = find(id)) return *r;
    throw NotFoundError(id);
}

const Resource& Catalog::get(const std::string& id) const {
    if (const Resource* r = find(id)) return *r;
    throw NotFoundError(id);
}

void Catalog::remove(const std::string& id) {
    if (items_.erase(id) == 0) throw NotFoundError(id);
}

void Catalog::addVendor(const std::string& id, std::string vendor, Money unitPrice) {
    get(id);
    if (vendor.empty() || unitPrice.isNegative()) throw std::invalid_argument("vendor and non-negative price required");
    if (unitPrice.currencyCode() != get(id).unitPrice().currencyCode())
        throw std::invalid_argument("vendor price currency must match resource currency");
    auto& offers = vendors_[id];
    for (auto& offer : offers) {
        if (offer.vendor == vendor) { offer.unitPrice = unitPrice; return; }
    }
    offers.push_back({std::move(vendor), unitPrice});
}

std::vector<VendorOffer> Catalog::vendorOffers(const std::string& id) const {
    get(id);
    auto it = vendors_.find(id);
    return it == vendors_.end() ? std::vector<VendorOffer>{} : it->second;
}

VendorOffer Catalog::cheapestVendor(const std::string& id) const {
    const Resource& r = get(id);
    auto offers = vendorOffers(id);
    if (offers.empty()) return {"Catalogue default", r.unitPrice()};
    return *std::min_element(offers.begin(), offers.end(), [](const auto& a, const auto& b) {
        return a.unitPrice < b.unitPrice;
    });
}

int Catalog::holdings(const std::string& id) const {
    auto it = items_.find(id);
    if (it == items_.end()) throw NotFoundError(id);
    return it->second.holdings;
}

void Catalog::addHoldings(const std::string& id, int units) {
    auto it = items_.find(id);
    if (it == items_.end()) throw NotFoundError(id);
    if (it->second.holdings + units < 0)
        throw std::invalid_argument("holdings cannot become negative");
    it->second.holdings += units;
}

std::vector<const Resource*> Catalog::where(
    const std::function<bool(const Resource&)>& pred) const {
    std::vector<const Resource*> out;
    for (const auto& [id, entry] : items_)
        if (pred(*entry.resource)) out.push_back(entry.resource.get());
    return out;
}

std::vector<const Resource*> Catalog::all() const {
    return where([](const Resource&) { return true; });
}

std::vector<const Resource*> Catalog::byCategory(ResourceCategory c) const {
    return where([c](const Resource& r) { return r.category() == c; });
}

std::vector<const Resource*> Catalog::searchTitle(const std::string& text) const {
    const std::string needle = lower(text);
    return where([&needle](const Resource& r) {
        return lower(r.title()).find(needle) != std::string::npos;
    });
}

std::vector<const Resource*> Catalog::searchAuthor(const std::string& author) const {
    const std::string needle = lower(author);
    return where([&needle](const Resource& r) {
        if (const auto* b = dynamic_cast<const Book*>(&r)) {
            for (const auto& a : b->authors()) if (lower(a).find(needle) != std::string::npos) return true;
        }
        if (const auto* e = dynamic_cast<const EBook*>(&r)) {
            for (const auto& a : e->authors()) if (lower(a).find(needle) != std::string::npos) return true;
        }
        return false;
    });
}

std::vector<const Resource*> Catalog::searchIdentifier(const std::string& identifier) const {
    const std::string needle = lower(identifier);
    return where([&needle](const Resource& r) {
        if (const auto* b = dynamic_cast<const Book*>(&r)) return lower(b->isbn()) == needle;
        if (const auto* e = dynamic_cast<const EBook*>(&r)) return lower(e->isbn()) == needle;
        if (const auto* j = dynamic_cast<const Journal*>(&r)) return lower(j->issn()) == needle;
        return false;
    });
}

std::vector<const Resource*> Catalog::searchYears(int firstYear, int lastYear) const {
    if (firstYear > lastYear) throw std::invalid_argument("first year must not exceed last year");
    return where([firstYear, lastYear](const Resource& r) { return r.year() >= firstYear && r.year() <= lastYear; });
}

void Catalog::borrow(const std::string& patron, const std::string& id) {
    Resource& r = get(id);
    if (r.isDigital()) throw std::invalid_argument("use openSession for electronic resources");
    auto key = std::make_pair(patron, id);
    int active = 0;
    for (const auto& [loanKey, count] : loans_) if (loanKey.second == id) active += count;
    if (active >= holdings(id)) throw std::invalid_argument("no available print copies");
    ++loans_[key];
}

void Catalog::returnCopy(const std::string& patron, const std::string& id) {
    get(id);
    auto it = loans_.find({patron, id});
    if (it == loans_.end() || it->second <= 0) throw std::invalid_argument("patron has no copy to return");
    if (--it->second == 0) loans_.erase(it);
}

void Catalog::openSession(const std::string& patron, const std::string& id) {
    Resource& r = get(id);
    if (!r.isDigital()) throw std::invalid_argument("openSession is only for electronic resources");
    auto key = std::make_pair(patron, id);
    int active = 0;
    for (const auto& [sessionKey, count] : sessions_) if (sessionKey.second == id) active += count;
    if (active >= holdings(id)) throw std::invalid_argument("no licensed seats available");
    ++sessions_[key];
}

void Catalog::closeSession(const std::string& patron, const std::string& id) {
    get(id);
    auto it = sessions_.find({patron, id});
    if (it == sessions_.end() || it->second <= 0) throw std::invalid_argument("patron has no open session");
    if (--it->second == 0) sessions_.erase(it);
}

int Catalog::borrowed(const std::string& patron, const std::string& id) const {
    auto it = loans_.find({patron, id}); return it == loans_.end() ? 0 : it->second;
}

int Catalog::openSessions(const std::string& patron, const std::string& id) const {
    auto it = sessions_.find({patron, id}); return it == sessions_.end() ? 0 : it->second;
}

}  // namespace bookmgmt
