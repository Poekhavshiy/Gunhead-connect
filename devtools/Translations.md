To make every user-facing string translatable in a Qt application, you need to follow these steps:

* * * * *

1\. **Wrap Strings in `tr()` or `QObject::tr()`**
-------------------------------------------------

-   In your C++ code, wrap all user-visible strings with `tr()` (if inside a QObject subclass) or `QObject::tr()` (if not).

    label->setText(tr("A sleek and modern theme with a clean design."));

-   For strings loaded from JSON (like your theme descriptions), you need to pass them through a translation function at runtime. For example:

    QString translatedDescription = tr(qPrintable(theme.description));

* * * * *

2\. **For JSON or External Files**
----------------------------------

-   You cannot extract strings from JSON directly with Qt's lupdate tool.
-   **Solution:**
    -   Add a C++ file (e.g., `theme_strings.cpp`) that contains all the strings you want to translate, wrapped in `tr()` or `QObject::tr()`.

    -   This file is only for translation extraction and not used at runtime.

        // theme_strings.cpp

        // Not compiled, only for lupdate!

        QObject::tr("A sleek and modern theme with a clean design.");

        QObject::tr("A theme inspired by a long, almost endless, very cold time period.");

        QObject::tr("Kawaii chibi tema-chan, so uWu to make your kokoro go doki doki!");

        QObject::tr("An empty expanse that just makes you want to run and parkour.");

        QObject::tr("A theme based on the Open Dracula standard.");

* * * * *

3\. **Run lupdate to Generate/Update .ts Files**
------------------------------------------------

-   In your project directory, run:

    lupdate . -ts resources/translations/lang_en.ts

-   This will scan your code (and the dummy file) for translatable strings and update the `.ts` file.

* * * * *

4\. **Translate Using Qt Linguist**
-----------------------------------

-   Open the `.ts` file in Qt Linguist and provide translations for each string.
-   Save and release as `.qm` files.

* * * * *

5\. **Load Translations at Runtime**
------------------------------------

-   In your `main.cpp`, load the appropriate `.qm` file using `QTranslator`.

* * * * *

6\. **At Runtime, Translate JSON Strings**
------------------------------------------

-   When displaying a string from JSON, use the same text as in your dummy file and call `tr()` on it:

    QString desc = tr(qPrintable(theme.description));

* * * * *

**Summary Table**
-----------------

| Step | What to do |
| --- | --- |
| 1 | Wrap all user strings in `tr()` or `QObject::tr()` in code |
| 2 | For JSON, add a dummy C++ file with all strings for extraction |
| 3 | Run `lupdate` to update `.ts` files |
| 4 | Translate with Qt Linguist |
| 5 | Load `.qm` at runtime |
| 6 | Use `tr()` on JSON strings at runtime |

* * * * *

**Tip:**\
Keep the dummy file updated as you add new user-facing strings to your JSON or other resources.

## Files to Translate
The following files contain user-facing strings that need to be translated. The strings are used in various UI elements, including buttons, labels, and messages.

MainWindow.cpp

Contains UI elements like buttons (Start Monitoring, Settings, View Log) and status labels (Ready, Monitoring started, etc.).
Displays messages in the updateStatusLabel method.
SettingsWindow.cpp

Contains UI elements for settings like Game LIVE Folder Path, KillAPI Key, and Check for Updates.
Displays messages in dialogs (e.g., Invalid Folder, KillAPI connected successfully, etc.).
ThemeSelect.cpp

Displays the theme selection UI with button labels for themes.
Includes the header label > THEME SELECTOR.
LanguageSelect.cpp

Displays the language selection UI with button labels for languages.
Includes the header labels > LANGUAGE SELECT and > FAN TRANSLATIONS.
LogDisplayWindow.cpp

Displays log events in the log display window.
Includes UI elements like checkboxes (Show PvP, Show PvE, etc.) and buttons (Clear, Text Color, etc.).
Displays status messages like Killfeed Log Display Initialized and Filter changed: Restart monitoring to apply.
checkversion.cpp

Displays messages in dialogs related to updates (e.g., Updates Available, No Updates, Update Successful, etc.).
Transmitter.cpp

Displays messages in dialogs when sending events (e.g., Debug ping successful, Failed to send connection success event, etc.).