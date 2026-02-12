Mining IPC
----------

The IPC mining interface now requires mining clients to use the latest `mining.capnp` schema. Clients built against older schemas will fail when calling `Init.makeMining` and receive an RPC error indicating the old mining interface is no longer supported. Mining clients must update to the latest schema and regenerate bindings to continue working. (#34568)

Notable IPC mining interface changes since the last release:
- `Mining.createNewBlock` and `Mining.checkBlock` now require a `context` parameter.
- `Mining.waitTipChanged` now has a default `timeout` (effectively infinite / `maxDouble`) if the client omits it.
- `BlockTemplate.getCoinbaseTx()` now returns a structured `CoinbaseTx` instead of raw bytes.
- Removed `BlockTemplate.getCoinbaseCommitment()` and `BlockTemplate.getWitnessCommitmentIndex()`.
- Cap’n Proto default values were updated to match the corresponding C++ defaults for mining-related option structs (e.g. `BlockCreateOptions`, `BlockWaitOptions`, `BlockCheckOptions`).
