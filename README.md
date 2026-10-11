# BookManagement — OOPD Assignment 2 (Monsoon 2026)

C++17 university-library catalogue and acquisition system extended for questions Q1–Q16 of the assignment. The implementation includes inheritance, polymorphic pricing, fixed-point money, quotas, budgets, batch acquisition, cancellation, departments, vendor selection, catalogue searches, and lending.

## Build and run

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/demo
```

The library is built with `-Wall -Wextra -Wpedantic` (on non-MSVC compilers). Tests use standard `assert` checks.

## Assignment coverage

- **Q1–Q3:** `Journal`, `EBook`, `AudioBook`, and `Thesis` resource types with category-specific metadata and pricing.
- **Q4–Q6:** Hardcover surcharge, print bulk discounts, tiered electronic-seat pricing, and configurable print/electronic taxes.
- **Q7:** Optional per-category distinct-title quota.
- **Q8:** Cancel an approved order; refund its budget/quota use, release holdings, and append a separate cancellation record while retaining the original.
- **Q9:** Register named department budgets and charge direct or batch purchases to a department.
- **Q10:** Create a next-year budget from a new allocation plus a configurable percentage of the unspent balance.
- **Q11:** Optional all-or-nothing batch processing with pre-validation.
- **Q12:** Multiple vendor offers per catalogue item; purchase and record the cheapest offer.
- **Q13:** Search by author, exact ISBN/ISSN, and inclusive publication-year range.
- **Q14:** Patron-specific print loans and electronic sessions limited by copies/seats held.
- **Q15:** Currency-coded `Money`; arithmetic and comparisons reject mismatched currencies.
- **Q16:** Test cases use standard `assert` and a typed exception assertion helper rather than the old custom check macros.

See [ANSWERS.md](ANSWERS.md) for the written design choices and question-by-question notes.

## Notes

- `Money` defaults to currency code `USD`; use `Money::of(major, minor, "INR")` or `Money::fromMinor(minor, "INR")` for another currency.
- Purchases with no department use the default **General** budget.
- Vendor prices are per unit. For resource types with tiered or surcharge pricing, the relevant pricing rules are applied to the selected vendor's unit price.
- The ZIP includes the local `.git` repository as required by the submission instructions. A GitHub remote is not configured in this environment; add your remote and push if required by the course.
