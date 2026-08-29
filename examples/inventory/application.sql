INSERT INTO _app_settings(key, value) VALUES
  ('title', 'Vulpes Inventory');

INSERT INTO _app_forms(name, table_name, label, is_default) VALUES
  ('product', 'products', 'Product', 1),
  ('stock-movement', 'stock_movements', 'Stock movement', 1);

INSERT INTO _app_form_fields(
  form_name, field_name, label, position, visible, read_only, format,
  lookup_display_field, lookup_search_fields, lookup_result_limit,
  lookup_allow_drill_down
) VALUES
  ('product', 'id', 'ID', 0, 0, 1, 'automatic', NULL, NULL, NULL, NULL),
  ('product', 'sku', 'SKU', 1, 1, 0, 'text', NULL, NULL, NULL, NULL),
  ('product', 'name', 'Product name', 2, 1, 0, 'text', NULL, NULL, NULL, NULL),
  ('product', 'category_id', 'Category', 3, 1, 0, 'automatic', 'name', '["name"]', 50, 1),
  ('product', 'supplier_id', 'Supplier', 4, 1, 0, 'automatic', 'name', '["name","email"]', 50, 1),
  ('product', 'reorder_level', 'Reorder level', 5, 1, 0, 'number', NULL, NULL, NULL, NULL),
  ('product', 'active', 'Active', 6, 1, 0, 'boolean', NULL, NULL, NULL, NULL),
  ('stock-movement', 'id', 'ID', 0, 0, 1, 'automatic', NULL, NULL, NULL, NULL),
  ('stock-movement', 'product_id', 'Product', 1, 1, 0, 'automatic', 'name', '["sku","name"]', 50, 1),
  ('stock-movement', 'location_id', 'Location', 2, 1, 0, 'automatic', 'code', '["code","name"]', 50, 1),
  ('stock-movement', 'quantity', 'Quantity', 3, 1, 0, 'number', NULL, NULL, NULL, NULL),
  ('stock-movement', 'occurred_at', 'Occurred', 4, 1, 0, 'date_time', NULL, NULL, NULL, NULL),
  ('stock-movement', 'note', 'Note', 5, 1, 0, 'text', NULL, NULL, NULL, NULL);

UPDATE _app_form_fields
SET time_zone = 'Europe/Prague'
WHERE form_name = 'stock-movement' AND field_name = 'occurred_at';

INSERT INTO _app_views(name, table_name, label, form_name) VALUES
  ('low-stock', 'low_stock', 'Low stock', NULL);

INSERT INTO _app_reports(name, label, sql, row_limit) VALUES
  ('low-stock', 'Low stock',
   'SELECT sku, name, quantity, reorder_level FROM low_stock ORDER BY sku', 1000);

INSERT INTO _app_commands(name, label, command) VALUES
  ('products', 'Products', 'form product'),
  ('stock-movements', 'Stock movements', 'form stock-movement'),
  ('low-stock', 'Low-stock report', 'report low-stock');

INSERT INTO _app_screens(name, label, description, is_default) VALUES
  ('home', 'Vulpes Inventory', 'Choose an inventory task.', 1);

INSERT INTO _app_screen_items(screen_name, position, label, description, command_name) VALUES
  ('home', 0, 'Products', 'Browse and maintain the product catalogue.', 'products'),
  ('home', 1, 'Stock movements', 'Record stock received, moved, or issued.', 'stock-movements'),
  ('home', 2, 'Low-stock report', 'Show products at or below their reorder level.', 'low-stock');

INSERT INTO _app_menus(name, label, position) VALUES
  ('inventory', 'Inventory', 0);

INSERT INTO _app_menu_items(menu_name, position, label, command_name) VALUES
  ('inventory', 0, 'Products', 'products'),
  ('inventory', 1, 'Stock movements', 'stock-movements'),
  ('inventory', 2, 'Low-stock report', 'low-stock');
