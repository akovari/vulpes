INSERT INTO categories(id, name) VALUES
  (1, 'Tools'),
  (2, 'Consumables');

INSERT INTO suppliers(id, name, email) VALUES
  (1, 'Acme Industrial', 'sales@acme.example'),
  (2, 'Northwind Supply', 'orders@northwind.example');

INSERT INTO locations(id, code, name) VALUES
  (1, 'MAIN', 'Main warehouse'),
  (2, 'SHOP', 'Workshop');

INSERT INTO products(id, sku, name, category_id, supplier_id, reorder_level, active) VALUES
  (1, 'HAM-001', 'Claw hammer', 1, 1, 5, 1),
  (2, 'PAP-500', 'Printer paper (500 sheets)', 2, 2, 10, 1),
  (3, 'OLD-001', 'Discontinued drill bit', 1, 1, 0, 0);

INSERT INTO stock_movements(product_id, location_id, quantity, occurred_at, note) VALUES
  (1, 1, 3, '2026-01-01T09:00:00Z', 'Opening stock'),
  (1, 2, -1, '2026-01-02T10:00:00Z', 'Issued to workshop'),
  (2, 1, 20, '2026-01-01T09:00:00Z', 'Opening stock'),
  (3, 1, 8, '2026-01-01T09:00:00Z', 'Archived item');
