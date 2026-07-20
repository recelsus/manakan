# Third-Party Notices

manakan vendors third-party single-header libraries in this repository.

Vendored source code:

- `include/toml.hpp`
  - `toml++` v3.4.0
  - Upstream: https://github.com/marzer/tomlplusplus
  - License: MIT

- `include/json.hpp`
  - `JSON for Modern C++` v3.11.3 (`nlohmann/json`)
  - Upstream: https://github.com/nlohmann/json
  - License: MIT
  - Upstream also notes that this bundled header contains portions taken from
    Google Abseil under the Apache License 2.0.

- `include/httplib.h`
  - `cpp-httplib` v0.15.3
  - Upstream: https://github.com/yhirose/cpp-httplib
  - License: MIT

System or package-manager provided libraries:

- OpenSSL may be linked when available through CMake or platform libraries.
  It is not vendored in this repository and remains under its own license.

Those components remain under their respective licenses. For the exact license
texts and attribution details of vendored dependencies, see the headers
themselves and the corresponding upstream projects.
