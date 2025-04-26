### **Flow of Events**

#### **1\. MainWindow Starts Monitoring**

-   **Trigger**: The user clicks the "Start Monitoring" button in the `MainWindow`.
-   **Action**:
    -   `MainWindow` calls `LogMonitor::startMonitoring()` with the path to the log file.
    -   `LogMonitor` opens the log file and begins monitoring it for changes.

* * * * *

#### **2\. LogMonitor Reads the Log File**

-   **Trigger**: A periodic timer in `LogMonitor` calls `LogMonitor::checkLogFile()`.
-   **Action**:
    -   `LogMonitor` reads new lines from the log file starting from the last read position.
    -   Each line is checked against a regex pattern (`m_regex`) to determine if it matches a known log format.
    -   If a line matches, `LogMonitor` emits the `logLineMatched` signal with the matched line wrapped in a JSON string:

        {"matched": "<raw log line>"}

* * * * *

#### **3\. MainWindow Handles the Emitted Log Line**

-   **Trigger**: The `logLineMatched` signal emitted by `LogMonitor` is connected to `MainWindow::handleLogLine()`.
-   **Action**:
    -   `MainWindow` receives the JSON string containing the matched log line.
    -   It parses the JSON to extract the `"matched"` field, which contains the raw log line.
    -   The raw log line is passed to `log_parser::parse_log_line()` for further processing.

* * * * *

#### **4\. log_parser Parses the Log Line**

-   **Trigger**: `MainWindow` calls `log_parser::parse_log_line()` with the raw log line.
-   **Action**:
    -   `log_parser` iterates through a list of regex rules ([rules](vscode-file://vscode-app/c:/Users/marce/AppData/Local/Programs/Microsoft%20VS%20Code/resources/app/out/vs/code/electron-sandbox/workbench/workbench.html)) loaded from a JSON file.
    -   Each rule is applied to the log line using `std::regex_search()`.
    -   If a rule matches, the log line is parsed into a structured JSON object with fields like `timestamp`, `victim`, `killer`, etc.
    -   The parsed JSON object is returned as a string.

* * * * *

#### **5\. MainWindow Sends the Parsed Log to Transmitter**

-   **Trigger**: `log_parser::parse_log_line()` returns the parsed JSON string to `MainWindow`.
-   **Action**:
    -   `MainWindow` passes the parsed log to `Transmitter::enqueueLog()` for further processing.
    -   `Transmitter` may send the log to an external API or perform other actions.

* * * * *

#### **6\. MainWindow Updates the LogDisplayWindow**

-   **Trigger**: After sending the parsed log to `Transmitter`, `MainWindow` updates the `LogDisplayWindow`.
-   **Action**:
    -   If the `LogDisplayWindow` is open, `MainWindow` calls `LogDisplayWindow::addEvent()` with the parsed log.
    -   `LogDisplayWindow` displays the log in its UI.

* * * * *

### **Diagram of Event Flow**
```dolphin
+----------------+       +----------------+       +----------------+       +----------------+       +--------------------+
|   MainWindow   |       |   LogMonitor   |       |   log_parser   |       |   Transmitter   |       | LogDisplayWindow   |
+----------------+       +----------------+       +----------------+       +----------------+       +--------------------+
        |                        |                        |                        |                        |
        | Start Monitoring       |                        |                        |                        |
        |----------------------->|                        |                        |                        |
        |                        | Read Log File          |                        |                        |
        |                        |----------------------->|                        |                        |
        |                        | Emit logLineMatched    |                        |                        |
        |                        |----------------------->|                        |                        |
        | Handle Log Line         |                        |                        |                        |
        |------------------------>|                        |                        |                        |
        | Parse Log Line          |                        |                        |                        |
        |------------------------>| Apply Regex Rules      |                        |                        |
        |                        |----------------------->|                        |                        |
        |                        | Return Parsed Log      |                        |                        |
        |                        |<-----------------------|                        |                        |
        | Send to Transmitter     |                        |                        |                        |
        |------------------------>|                        |                        |                        |
        |                        |                        | Enqueue Log            |                        |
        |                        |                        |----------------------->|                        |
        | Update LogDisplayWindow |                        |                        |                        |
        |------------------------>|                        |                        |                        |
        |                        |                        |                        | Display Log in UI       |
        |                        |                        |                        |------------------------>|
```

* * * * *

### **Key Components and Their Roles**

#### **1\. MainWindow**

-   Acts as the central controller of the application.
-   Starts and stops log monitoring by interacting with `LogMonitor`.
-   Handles log lines emitted by `LogMonitor` and passes them to `log_parser` for processing.
-   Sends parsed logs to `Transmitter` and updates the `LogDisplayWindow` to show the logs.

* * * * *

#### **2\. LogMonitor**

-   Monitors the log file for changes.
-   Reads new lines from the log file and checks them against a regex pattern.
-   Emits the `logLineMatched` signal when a line matches the regex, wrapping the line in a JSON string.

* * * * *

#### **3\. log_parser**

-   Parses raw log lines into structured JSON objects.
-   Uses regex rules loaded from a JSON file to match and extract fields from log lines.
-   Handles different types of events (e.g., `kill_log`, `vehicle_destruction`) and formats them into a consistent JSON structure.

* * * * *

#### **4\. Transmitter**

-   Handles the transmission of parsed logs to an external system (e.g., an API or a server).
-   Queues logs for processing and ensures they are sent reliably.

* * * * *

#### **5\. LogDisplayWindow**

-   Displays parsed logs in a user-friendly format.
-   Receives parsed logs from `MainWindow` and updates its UI to show the events.

* * * * *

### **Flow Summary**

1.  **Start Monitoring**:

    -   The user starts monitoring in `MainWindow`, which triggers `LogMonitor` to begin reading the log file.
2.  **Log File Changes**:

    -   `LogMonitor` detects new lines in the log file, checks them against regex patterns, and emits the `logLineMatched` signal for matching lines.
3.  **Handle Log Line**:

    -   `MainWindow` receives the emitted log line, extracts the raw log line, and passes it to `log_parser`.
4.  **Parse Log Line**:

    -   `log_parser` parses the raw log line into a structured JSON object using regex rules.
5.  **Transmit and Display**:

    -   `MainWindow` sends the parsed log to `Transmitter` for external processing and updates `LogDisplayWindow` to display the log.