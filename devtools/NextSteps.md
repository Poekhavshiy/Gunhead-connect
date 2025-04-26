### ✅ **Core Groundwork Suggestions (in order of impact/effort)**

#### 1\. **Unit Test Coverage Expansion**

-   ✅ You have Catch2 set up.

-   📌 **Next step**: Add more test cases for `log_parser`:

    -   Invalid/malformed log lines

    -   Different kill log formats (e.g., with timestamps, special characters)

    -   Edge cases (empty lines, null inputs)

#### 2\. **Logging & Diagnostics**

-   Add a minimal logging utility (e.g., `log_debug(...)`, `log_error(...)`) that works in both app and test contexts.

-   Print what's being parsed, sent, or matched --- useful during development.

#### 3\. **Error Handling Layer**

-   Define clean error-handling behavior for:

    -   Failed regex matches

    -   Malformed JSON output

    -   API failures (in future REST integration)

-   Maybe even define an error enum/class.

#### 4\. **Basic Configuration System**

-   Load config (e.g., regex patterns, API endpoints) from a JSON file.

-   In test mode, mock or override config loading.

#### 5\. **CI Integration (Simple to Start)**

-   If you haven't already: ✅ use GitHub Actions or GitLab CI to run:

    -   `cmake --preset windows-dynamic`

    -   `cmake --build ...`

    -   `ctest --preset test`

-   No need to publish artifacts yet --- just automate testing on push/PR.
-   Use Git Large File Storage (LFS) to replace the large files from Qt6

#### 6\. **Versioning & Metadata**

-   Add a version header and `--version` CLI flag.

-   Embed Git commit hash, build date, etc. via `configure_file()` or `#define` macros.

* * * * *

### 🔜 Coming Up Soon

#### 7\. **API Client Module**

-   Create a stub that accepts JSON and simulates a REST POST (stdout or file).

-   Wrap this in a testable abstraction so later you can replace with real networking.

#### 8\. **Live Tailing Logic (if not done yet)**

-   Add basic file tailing with `std::ifstream` in polling mode (later: consider OS file watching).

-   Write tests to simulate appending lines to a file and checking parser output.

#### 9\. **Integration Tests**

-   Full flow: simulated `game.log` ➜ parser ➜ JSON ➜ stubbed API call.

-   Can use temp files or stringstream mocking.


<sub>
- Want me to help scaffold or stub any of these out? I can drop you a sample config loader, logging class, or test skeleton depending
</sub>
<br>
<sub>
- Copy imageformats/, styles/, or other plugin folders like for SVG or networking support
</sub>
<sub> Rebuild Qt with OpenSSL Support</sub>