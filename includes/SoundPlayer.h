#pragma once

#include <QString>
#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSettings>
#include <QDebug>
#include <QFile>
#include <QTemporaryFile>  // Add this line
#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>
#include <QTimer>
#include <QTextEdit>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

class SoundPlayer : public QObject {
    Q_OBJECT

public:
    explicit SoundPlayer(QObject* parent = nullptr);
    ~SoundPlayer();

    // Play a sound with logging output to specified text widget (if provided)
    void playSound(const QString& soundFile, QObject* logTarget = nullptr);
    
    // Play the sound configured in settings
    void playConfiguredSound(QObject* logTarget = nullptr);
    
    // Simple sound test without diagnostic output
    void playSimpleSound(const QString& soundFile);

    // Pre-initialize the audio subsystem to prevent lag on first playback
    static void preInitializeAudio();
        
    QStringList getAvailableSoundFiles();

signals:
    void soundPlaybackStarted();
    void soundPlaybackFinished();
    void playbackError(const QString& errorMessage);
    void diagnosticMessage(const QString& message); // For log output


private:
    bool initializePlayer(const QString& soundFile);
    
    QMediaPlayer* player = nullptr;
    QAudioOutput* audioOutput = nullptr;
    bool* soundPlaybackSuccessfulPtr = nullptr;

    // File validation constants
    static constexpr qint64 MAX_SOUND_FILE_SIZE = 5 * 1024 * 1024; // 5MB max file size
    static constexpr int MAX_SOUND_DURATION_MS = 5000; // 5 seconds max duration

    // File validation methods
    bool validateSoundFile(const QString& filePath, QTextEdit* logDisplay = nullptr);
    bool checkFileSize(const QString& filePath, QTextEdit* logDisplay = nullptr);
    bool checkFileFormat(const QString& filePath, QTextEdit* logDisplay = nullptr);
    

    // Timer for enforcing max duration
    QTimer* durationLimitTimer = nullptr;
};