# OOPD Assignment 2 — Answers and Design Decisions

## Questions attempted

Implemented Q1–Q16 in the project source, tests, and demo.

## Design decisions and written answers

- **Q1 — Journal:** `Journal` derives directly from `Resource` because it is a print subscription with its own ISSN and subscription period. Its price is annual per copy, multiplied by copies and subscription years; the print bulk discount is applied at ten or more copies.
- **Q2 — EBook:** `EBook` derives from `ElectronicResource` to reuse digital-seat pricing, access URL, licence model, and platform fee while keeping a separate quota category. The duplicated metadata between `Book` and `EBook` is authors and ISBN, along with their display logic. A shared `BookMetadata` composition object or a common abstract book-like base class could remove this duplication.
- **Q3 — AudioBook and Thesis:** `AudioBook` derives from `Book` to reuse authors, ISBN, edition, and related work metadata; it adds narrator and duration and overrides pricing so print-only bulk discounts do not apply. `Thesis` derives from `Resource` because it is an academic work with university, degree, and supervisor metadata, not inherently a print copy or a digital-seat licence. Its default price is zero.
- **Q4 — Hardcover:** Hardcover books add a 20% surcharge before any applicable bulk discount.
- **Q5 — Bulk discounts:** Print books and journals receive a 10% discount at ten or more copies. Electronic resources charge full price for the first 50 seats and half price for each additional seat.
- **Q6 — Taxes:** Print and digital tax percentages are configurable. Quotes and reports distinguish pre-tax amount, tax, and post-tax amount; quota checks use post-tax cost.
- **Q7 — Title quota:** A quota may cap distinct resource IDs per category in addition to units and spend.
- **Q8 — Cancellation:** Cancellation creates a separate history record, refunds budget and category usage, releases the title quota when its final purchase is refunded, and reduces holdings. The original order remains in history.
- **Q9 — Department budgets:** Departments are registered with their own `Budget`; purchase requests and direct purchases can name the department charged. Requests without a department use the default General budget.
- **Q10 — Year-end rollover:** `createNextYear(newAllocation, carryForwardPercent)` adds the configured percentage of this year's unspent amount to the new allocation and carries forward the category quota definitions.
- **Q11 — Atomic batches:** `processBatch(requests, true)` validates the full batch against simulated budgets before committing. If any request would fail, all requests are recorded as rejected and no holdings or budgets are changed.
- **Q12 — Vendors:** Each catalogue item can have multiple vendor offers. Acquisition quotes and purchases use the lowest unit-price offer, and order history records the selected vendor.
- **Q13 — Searches:** Catalogue search supports case-insensitive author matching, exact ISBN/ISSN matching, and inclusive publication-year ranges.
- **Q14 — Lending:** Print loans are limited by holdings; electronic sessions are limited by licensed-seat holdings. Borrow/return and open/close operations are tracked per patron and resource.
- **Q15 — Currency:** `Money` carries a currency code (USD by default). Arithmetic and comparisons between different currency codes throw `std::invalid_argument`; currency is retained by calculations and displayed in reports.
- **Q16 — Assertions:** The previous custom `CHECK` macros have been removed. Tests use standard `assert` and a small typed exception-checking helper that itself asserts the expected exception.

## Build and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/demo
```

The `.git` directory is included in the submitted ZIP. No GitHub remote was configured from this environment; add your repository remote and push if your course submission requires the project to be hosted on GitHub.
