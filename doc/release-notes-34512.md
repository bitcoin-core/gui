Updated RPCs
------------

- The `getblock` RPC now returns a `coinbase_tx` object at verbosity levels 1, 2,
  and 3. It contains `version`, `locktime`, `sequence`, `coinbase` and
  `witness`. This allows for efficiently querying coinbase
  transaction properties without fetching the full transaction data at
  verbosity 2+. (#34512)
