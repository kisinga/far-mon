# shared/ — Protocols and Utilities

All serialization, deserialization, and protocol logic used across edge and server components.

- Packing/unpacking code for sensor and config packets (Python, C++, etc.)
- Message versioning and address map.
- Common utilities and test vectors for both firmware and bridge code.

**Extending:**  
- Update or add protocol definitions here as the system evolves.
- All devices and bridges must conform to definitions here.

**See:**  
- `protocol.md` for message structure and version history.
