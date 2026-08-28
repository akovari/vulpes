# Vulpes example applications

The example databases are ordinary SQLite scripts. They contain no private
Vulpes format and remain usable with any SQLite tool. Automated acceptance tests
load them into in-memory databases so the same application-model workflows run
on every build host without writing repository files.

## Inventory dogfood application

`inventory/schema.sql` defines products, categories, suppliers, locations,
stock movements, and a low-stock view. `inventory/seed.sql` supplies a compact
representative data set. The automated scenario proves that the generic runtime
can:

- render the low-stock SQL result through the shared Grid;
- browse and transactionally edit a product;
- search a foreign-key relationship and inspect the related category;
- select product and location keys through display values;
- record a stock movement with an RFC 3339 UTC default timestamp; and
- refresh the low-stock view after the write.

Create a persistent copy on Windows with the optional SQLite CLI:

```powershell
sqlite3 inventory.db ".read examples/inventory/schema.sql"
sqlite3 inventory.db ".read examples/inventory/seed.sql"
.\build\windows-msvc\Debug\vulpes.exe inventory.db
```

Then browse `products`, edit the hammer, open its Category or Supplier lookup,
browse `stock_movements`, insert a movement, and run
`SELECT * FROM low_stock` in the SQL console.

## Workshop success scenario

`workshop/schema.sql` is the small customers/jobs database from the product
success definition. Its automated scenario browses, inserts, and edits a
customer; creates a job through a searched customer relationship; filters and
searches open jobs; follows the relationship; and renders a joined SQL query in
Grid.

```powershell
sqlite3 workshop.db ".read examples/workshop/schema.sql"
sqlite3 workshop.db ".read examples/workshop/seed.sql"
.\build\windows-msvc\Debug\vulpes.exe workshop.db
```

These deterministic tests demonstrate portable application semantics. They do
not replace the separate physical-terminal acceptance checks tracked in
`TODO.md` for Windows, Linux, and macOS.
