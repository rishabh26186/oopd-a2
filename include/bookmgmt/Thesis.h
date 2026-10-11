#pragma once

#include <string>

#include "bookmgmt/Resource.h"

namespace bookmgmt {

// A thesis is a distinct academic resource rather than a print book or online
// seat licence, so it derives directly from Resource. Its default price is zero.
class Thesis : public Resource {
public:
    Thesis(std::string id, std::string title, std::string publisher, int year,
           std::string university, std::string degree, std::string supervisor,
           Money unitPrice = Money{});

    const std::string& university() const { return university_; }
    const std::string& degree() const { return degree_; }
    const std::string& supervisor() const { return supervisor_; }
    ResourceCategory category() const override { return ResourceCategory::Thesis; }

protected:
    void printDetails(std::ostream& os) const override;

private:
    std::string university_;
    std::string degree_;
    std::string supervisor_;
};

}  // namespace bookmgmt
