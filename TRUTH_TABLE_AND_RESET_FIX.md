# Unit truth table and vending reset corrections

- `tests/unit/TestUnit.cpp`: AND, OR and XOR Unit Tests each evaluate all four binary input combinations (00, 01, 10, 11) and record PASS only if every row is correct. The named-test count does not change.
- `gui/app.js`: Reset session clears coins, credit, vending output and assertion display to 0 / NOT VERIFIED without running a fresh vending simulation. Outstanding asynchronous requests are invalidated so they cannot replace the cleared results. Inserting a new coin runs the existing backend assertion checks as before.

Run `make simulator` and `./simulator --test all`. Launch GUI with `python3 gui/server.py 8080`, click Reset session and then Insert 5 or Insert 10.
