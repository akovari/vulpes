INSERT INTO customers(id, name, phone) VALUES
  (1, 'Acme Ltd', '+420 555 0100'),
  (2, 'Delta Systems', '+420 555 0200');

INSERT INTO jobs(id, customer_id, description, status) VALUES
  (1, 1, 'Replace workshop lights', 'open'),
  (2, 2, 'Service air compressor', 'closed');
