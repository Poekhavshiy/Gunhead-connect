#include "SoundPlayer.h"
#include <QApplication>
#include <QMetaMethod>

SoundPlayer::SoundPlayer(QObject* parent) : QObject(parent) {
    // Initialize pointers to null
    player = nullptr;
    audioOutput = nullptr;
    soundPlaybackSuccessfulPtr = nullptr;
}

SoundPlayer::~SoundPlayer() {
    if (player) {
        player->stop();
        player->deleteLater();
    }
    
    if (audioOutput) {
        audioOutput->deleteLater();
    }
    
    // Clean up the flag if it exists
    if (soundPlaybackSuccessfulPtr) {
        delete soundPlaybackSuccessfulPtr;
    }
}

// Validate sound file before playing
bool SoundPlayer::validateSoundFile(const QString& filePath, QTextEdit* logDisplay) {
    // Skip validation for resource files (built-in sounds)
    if (filePath.startsWith(":/")) {
        return true;
    }
    
    // Check file size
    if (!checkFileSize(filePath, logDisplay)) {
        return false;
    }
    
    // Check file format
    if (!checkFileFormat(filePath, logDisplay)) {
        return false;
    }
    
    return true;
}

// Check if file size is within limits
bool SoundPlayer::checkFileSize(const QString& filePath, QTextEdit* logDisplay) {
    QFileInfo fileInfo(filePath);
    
    if (!fileInfo.exists()) {
        qWarning() << "Sound file does not exist:" << filePath;
        if (logDisplay) {
            logDisplay->append(QString("ERROR: Sound file does not exist: %1").arg(filePath));
        }
        return false;
    }
    
    qint64 size = fileInfo.size();
    
    if (size > MAX_SOUND_FILE_SIZE) {
        qWarning() << "Sound file exceeds maximum size:" << filePath 
                  << "Size:" << size << "bytes"
                  << "Max:" << MAX_SOUND_FILE_SIZE << "bytes";
        if (logDisplay) {
            logDisplay->append(QString("ERROR: Sound file exceeds maximum size (%1 MB): %2")
                              .arg(MAX_SOUND_FILE_SIZE / 1024.0 / 1024.0, 0, 'f', 2)
                              .arg(filePath));
        }
        return false;
    }
    
    return true;
}

// Check if file format is valid
bool SoundPlayer::checkFileFormat(const QString& filePath, QTextEdit* logDisplay) {
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open sound file for validation:" << filePath;
        if (logDisplay) {
            logDisplay->append(QString("ERROR: Cannot open sound file for validation: %1").arg(filePath));
        }
        return false;
    }
    
    // Read file header for validation
    QByteArray header = file.read(12);
    file.close();
    
    bool isValid = false;
    QString format = "Unknown";
    
    if (filePath.toLower().endsWith(".mp3")) {
        // Check for MP3 header (ID3 or MPEG frame sync)
        if (header.startsWith("ID3") || 
            (header.size() >= 2 && (header[0] == '\xFF') && ((header[1] & 0xE0) == 0xE0))) {
            isValid = true;
            format = "MP3";
        }
    } 
    else if (filePath.toLower().endsWith(".wav")) {
        // Check for RIFF WAVE header
        if (header.startsWith("RIFF") && header.mid(8, 4) == "WAVE") {
            isValid = true;
            format = "WAV";
        }
    }
    
    if (!isValid) {
        qWarning() << "Invalid sound file format:" << filePath;
        if (logDisplay) {
            logDisplay->append(QString("ERROR: Invalid sound file format. Expected %1 format but file appears corrupt or invalid.")
                              .arg(filePath.toLower().endsWith(".mp3") ? "MP3" : "WAV"));
        }
        return false;
    }
    
    qDebug() << "Validated sound file:" << filePath << "Format:" << format;
    return true;
}

void SoundPlayer::playSound(const QString& soundFile, QObject* logTarget) {
    QTextEdit* logDisplay = qobject_cast<QTextEdit*>(logTarget);
    
    // Log start of diagnostics
    if (logDisplay) {
        logDisplay->append("--- SOUND DIAGNOSTIC START ---");
    }
    qDebug() << "=== SOUND DIAGNOSTIC START ===";

    // 1. Check if the sound file exists and print details
    bool fileExists = soundFile.startsWith(":/") ? 
        QFile::exists(soundFile) : QFileInfo(soundFile).exists();
    
    qDebug() << "Sound file path:" << soundFile;
    qDebug() << "Sound file exists:" << fileExists;
    qDebug() << "Sound file absolute path:" << QFileInfo(soundFile).absoluteFilePath();
    
    if (logDisplay) {
        logDisplay->append(QString("Sound file: %1 (exists: %2)").arg(soundFile).arg(fileExists ? "Yes" : "No"));
    }
    
    QString soundFilePath = soundFile;
    
    if (!fileExists) {
        qWarning() << "Sound file doesn't exist, trying system directory...";
        // Try to find sound in executable directory
        QString exeDir = QCoreApplication::applicationDirPath();
        QString altPath = exeDir + "/sounds/" + QFileInfo(soundFile).fileName();
        bool altExists = QFileInfo(altPath).exists();
        qDebug() << "Alternative sound path:" << altPath;
        qDebug() << "Alternative exists:" << altExists;
        
        if (altExists) {
            soundFilePath = altPath;
            qDebug() << "Using alternative sound file:" << soundFilePath;
            if (logDisplay) {
                logDisplay->append(QString("Using alternative sound file: %1").arg(soundFilePath));
            }
        }
    }
    
    // 2. Check Qt Multimedia plugins
    QString pluginsDir = QCoreApplication::applicationDirPath() + "/multimedia";
    QDir dir(pluginsDir);
    QStringList plugins = dir.entryList(QDir::Files);
    qDebug() << "Qt Multimedia plugins directory:" << pluginsDir;
    qDebug() << "Available plugins:" << plugins;
    
    if (logDisplay) {
        logDisplay->append(QString("Multimedia plugins: %1").arg(plugins.isEmpty() ? "None found" : plugins.join(", ")));
    }
    
    // 3. Create and configure player with extensive error reporting
    player = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    
    // Use std::shared_ptr for automatic memory management
    std::shared_ptr<bool> soundPlaybackSuccessfulPtr = std::make_shared<bool>(false);
    
    // Connect to all possible signals for diagnostic purposes
    connect(player, &QMediaPlayer::errorOccurred, this, [this, soundFilePath, logDisplay](QMediaPlayer::Error error, const QString &errorString) {
        QString errorMsg = QString("Media error %1: %2").arg(error).arg(errorString);
        qWarning() << errorMsg;
        emit playbackError(errorMsg);
        
        if (logDisplay) {
            logDisplay->append(errorMsg);
        }
        
        // Report file information during error
        if (soundFilePath.startsWith(":/")) {
            QFile resFile(soundFilePath);
            qDebug() << "Resource file open:" << resFile.open(QIODevice::ReadOnly);
            if (resFile.isOpen()) {
                qDebug() << "Resource file size:" << resFile.size() << "bytes";
                resFile.close();
            }
        } else {
            QFile file(soundFilePath);
            qDebug() << "File open:" << file.open(QIODevice::ReadOnly);
            if (file.isOpen()) {
                qDebug() << "File size:" << file.size() << "bytes";
                file.close();
            }
        }
        
        // Windows fallback after error
        #ifdef Q_OS_WIN
        if (logDisplay) {
            logDisplay->append("Trying Windows system sound...");
        }
        PlaySound(TEXT("SystemExclamation"), NULL, SND_ALIAS | SND_ASYNC);
        #endif
    });
    
    connect(player, &QMediaPlayer::playbackStateChanged, this, [this, logDisplay, soundPlaybackSuccessfulPtr](QMediaPlayer::PlaybackState state) {
        QString stateStr;
        switch (state) {
            case QMediaPlayer::PlayingState: 
                stateStr = "Playing"; 
                *soundPlaybackSuccessfulPtr = true; // Set via pointer
                emit soundPlaybackStarted();
                break;
            case QMediaPlayer::PausedState: stateStr = "Paused"; break;
            case QMediaPlayer::StoppedState: stateStr = "Stopped"; break;
        }
        qDebug() << "Playback state changed:" << stateStr;
        
        if (logDisplay) {
            logDisplay->append(QString("Playback state: %1").arg(stateStr));
        }
        
        if (state == QMediaPlayer::StoppedState) {
            if (logDisplay) {
                logDisplay->append("Sound playback completed");
            }
            emit soundPlaybackFinished();
        }
    });
    
    // Additional diagnostics with mediaStatusChanged
    connect(player, &QMediaPlayer::mediaStatusChanged, this, 
    [this, soundPlaybackSuccessfulPtr](QMediaPlayer::MediaStatus status) {
        qDebug() << "Media status changed:" << status;
        if (status == QMediaPlayer::InvalidMedia || status == QMediaPlayer::NoMedia) {
            // Mark as unsuccessful so the fallback is triggered
            if (soundPlaybackSuccessfulPtr) {
                *soundPlaybackSuccessfulPtr = false;
            }
        }
    });
    
    // 4. Try alternative sound file formats
    if (soundFilePath.endsWith(".mp3")) {
        // Also try WAV format
        QString wavFile = soundFilePath;
        wavFile.replace(".mp3", ".wav");
        if (QFile::exists(wavFile)) {
            soundFilePath = wavFile;
            qDebug() << "Switched to WAV format:" << soundFilePath;
            
            if (logDisplay) {
                logDisplay->append(QString("Trying WAV format: %1").arg(soundFilePath));
            }
        }
    }
    
    // 5. Set source and play with diagnostics
    if (soundFilePath.startsWith(":/")) {
        // Convert the resource path to a proper qrc:// URL
        QString qrcPath = soundFilePath;
        qrcPath.replace(":/", "qrc:/");
        QUrl url = QUrl(qrcPath);
        qDebug() << "Setting resource URL:" << url.toString();
        
        if (logDisplay) {
            logDisplay->append(QString("Resource URL: %1").arg(url.toString()));
        }
        
        player->setSource(url);
    } else {
        QUrl url = QUrl::fromLocalFile(soundFilePath);
        qDebug() << "Setting local file URL:" << url.toString();
        
        if (logDisplay) {
            logDisplay->append(QString("Local file URL: %1").arg(url.toString()));
        }
        
        player->setSource(url);
    }
    
    player->setAudioOutput(audioOutput);
    audioOutput->setVolume(1.0);
    
    // Log before playing
    qDebug() << "Starting playback...";
    
    if (logDisplay) {
        logDisplay->append("Starting sound playback...");
    }
    
    // Validate sound file before playing
    if (!validateSoundFile(soundFilePath, logDisplay)) {
        qWarning() << "Sound file validation failed:" << soundFilePath;
        if (logDisplay) {
            logDisplay->append("Sound file validation failed - using system beep instead");
            logDisplay->append("--- SOUND DIAGNOSTIC END ---");
        }
        QApplication::beep();
        return;
    }
    
    player->play();
    
    // Set up duration limit timer
    if (durationLimitTimer) {
        durationLimitTimer->stop();
        delete durationLimitTimer;
    }
    
    durationLimitTimer = new QTimer(this);
    durationLimitTimer->setSingleShot(true);
    connect(durationLimitTimer, &QTimer::timeout, this, [this, logDisplay]() {
        if (player && player->playbackState() == QMediaPlayer::PlayingState) {
            qDebug() << "Sound duration limit reached (5 seconds) - stopping playback";
            if (logDisplay) {
                logDisplay->append("Sound duration limit reached (5 seconds) - stopping playback");
            }
            player->stop();
        }
    });
    
    // Start duration limit timer
    durationLimitTimer->start(MAX_SOUND_DURATION_MS);
    
    // Post-play diagnostics
    qDebug() << "Playback requested";
    qDebug() << "Media source:" << player->source().toString();
    qDebug() << "Player state:" << player->playbackState();
    qDebug() << "Audio volume:" << audioOutput->volume();
    qDebug() << "Audio muted:" << audioOutput->isMuted();
    
    // Timeout to detect silent failures
    QTimer::singleShot(500, this, [this, logDisplay, soundPlaybackSuccessfulPtr]() {
        // Only show warning if playback never started
        if (!*soundPlaybackSuccessfulPtr) {
            qDebug() << "DIAGNOSTIC: Playback didn't start within 500ms";
            
            if (logDisplay) {
                logDisplay->append("WARNING: Playback didn't start - silent failure detected");
            }
            
            #ifdef Q_OS_WIN
            // Try direct file playback via Windows API as last resort
            QString directFile = player->source().toLocalFile();
            if (!directFile.isEmpty() && logDisplay) {
                logDisplay->append(QString("Trying direct Windows API playback: %1").arg(directFile));
                PlaySound(directFile.toStdWString().c_str(), NULL, SND_FILENAME | SND_ASYNC);
            }
            #endif
        }
    });
    
    // Last resort beep after a delay if playback failed
    QTimer::singleShot(1000, this, [this, logDisplay, soundPlaybackSuccessfulPtr]() {
        // Check for null before dereferencing
        if (soundPlaybackSuccessfulPtr && !*soundPlaybackSuccessfulPtr) {
            qDebug() << "System beep as fallback (sound playback failed)";
            
            if (logDisplay) {
                logDisplay->append("System beep fallback (sound playback failed)");
            }
            
            QApplication::beep();
        }
    });

    if (logDisplay) {
        logDisplay->append("--- SOUND DIAGNOSTIC END ---");
    }
    
    qDebug() << "=== SOUND DIAGNOSTIC END ===";
}

void SoundPlayer::playConfiguredSound(QObject* logTarget) {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    QString soundFile = settings.value("LogDisplay/SoundFile", ":/sounds/beep-beep.mp3").toString();
    playSound(soundFile, logTarget);
}

void SoundPlayer::playSimpleSound(const QString& soundFile) {
    if (initializePlayer(soundFile)) {
        player->play();
    }
}

bool SoundPlayer::initializePlayer(const QString& soundFile) {
    // Stop any pending player operations before cleanup
    if (player) {
        // Disconnect all signals from this player to prevent callbacks after deletion
        player->disconnect();
        
        // Check if player is in a valid state before stopping
        // Don't call stop() directly as this can crash if player is in an unstable state
        if (player->playbackState() != QMediaPlayer::StoppedState) {
            // Use a safer approach - set volume to 0 first
            if (audioOutput) {
                audioOutput->setVolume(0.0);
            }
            // Then schedule player to be stopped and deleted
            QTimer::singleShot(0, player, &QMediaPlayer::stop);
        }
        
        // Schedule deletion but don't reference after this point
        player->deleteLater();
        player = nullptr;
    }
    
    if (audioOutput) {
        audioOutput->disconnect();
        audioOutput->deleteLater();
        audioOutput = nullptr;
    }
    
    // Create new player with a short delay to ensure old one is gone
    QTimer timer;
    QEventLoop loop;
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(10);  // Small delay to process pending delete operations
    loop.exec();
    
    // Now create fresh instances
    player = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    
    // Basic error handling
    connect(player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &errorString) {
        qWarning() << "Sound playback error:" << error << "-" << errorString;
        emit playbackError(errorString);
    });
    
    // Handle playback state changes
    connect(player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::PlayingState) {
            emit soundPlaybackStarted();
        } else if (state == QMediaPlayer::StoppedState) {
            emit soundPlaybackFinished();
            // Don't delete the player immediately after it stops
            // Instead, schedule deletion after a delay
            QTimer::singleShot(500, this, [this]() {
                // Check if this player is still our current player
                if (this->player) {
                    // Only nullify if it's our current player
                    this->player = nullptr;
                }
                // Delete it regardless
                if (this->player)
                    this->player->deleteLater();
            });
        }
    });
    
    // Set the source based on path type
    if (soundFile.startsWith(":/")) {
        QString qrcPath = soundFile;
        qrcPath.replace(":/", "qrc:/");
        player->setSource(QUrl(qrcPath));
    } else {
        player->setSource(QUrl::fromLocalFile(soundFile));
    }
    
    player->setAudioOutput(audioOutput);
    audioOutput->setVolume(0.5);
    
    return true;
}

// Add this implementation:
void SoundPlayer::preInitializeAudio() {
    qDebug() << "Pre-initializing audio subsystem...";
    
    // Create a temporary player and audio output to initialize the audio subsystem
    QMediaPlayer* warmupPlayer = new QMediaPlayer();
    QAudioOutput* warmupOutput = new QAudioOutput();
    warmupPlayer->setAudioOutput(warmupOutput);
    
    // Create a 10ms silent audio file in memory
    QByteArray silentData;
    silentData.append(QByteArray::fromHex("52494646")); // "RIFF"
    silentData.append(QByteArray::fromHex("24000000")); // ChunkSize
    silentData.append(QByteArray::fromHex("57415645")); // "WAVE"
    silentData.append(QByteArray::fromHex("666D7420")); // "fmt "
    silentData.append(QByteArray::fromHex("10000000")); // Subchunk1Size
    silentData.append(QByteArray::fromHex("0100")); // AudioFormat (PCM)
    silentData.append(QByteArray::fromHex("0100")); // NumChannels (mono)
    silentData.append(QByteArray::fromHex("44AC0000")); // SampleRate (44100)
    silentData.append(QByteArray::fromHex("88580100")); // ByteRate (44100*2)
    silentData.append(QByteArray::fromHex("0200")); // BlockAlign
    silentData.append(QByteArray::fromHex("1000")); // BitsPerSample (16)
    silentData.append(QByteArray::fromHex("64617461")); // "data"
    silentData.append(QByteArray::fromHex("00000000")); // Subchunk2Size (0 - no samples)

    // Create a temporary file with silent audio
    QTemporaryFile* tempFile = new QTemporaryFile();
    if (tempFile->open()) {
        tempFile->write(silentData);
        tempFile->flush();
        
        // Set source and play silently
        warmupOutput->setVolume(0.0);
        warmupPlayer->setSource(QUrl::fromLocalFile(tempFile->fileName()));
        warmupPlayer->play();
        
        // Wait briefly for initialization, then clean up
        QTimer::singleShot(100, [warmupPlayer, warmupOutput, tempFile]() {
            warmupPlayer->stop();
            warmupPlayer->deleteLater();
            warmupOutput->deleteLater();
            tempFile->close();
            tempFile->deleteLater();
            qDebug() << "Audio subsystem pre-initialization complete";
        });
    } else {
        // Clean up on failure
        delete warmupPlayer;
        delete warmupOutput;
        delete tempFile;
    }
    
    // Also pre-load the Windows audio APIs if on Windows
    #ifdef Q_OS_WIN
    // Make a silent Windows API sound call to warm up that subsystem too
    PlaySound(NULL, NULL, SND_ASYNC | SND_NODEFAULT | SND_NOSTOP | SND_NOWAIT);
    #endif
}

// Add this method to SoundPlayer class:
QStringList SoundPlayer::getAvailableSoundFiles() {
    QStringList soundFiles;
    
    // Add built-in resource sounds
    QDir resourceDir(":/sounds");
    QStringList resourceSounds = resourceDir.entryList(QDir::Files);
    for (const QString& sound : resourceSounds) {
        soundFiles.append(":/sounds/" + sound);
    }
    
    // Add custom sounds from application directory
    QString appSoundsDir = QCoreApplication::applicationDirPath() + "/sounds";
    QDir customDir(appSoundsDir);
    
    // Look for both .mp3 and .wav files
    QStringList filters;
    filters << "*.mp3" << "*.wav";
    customDir.setNameFilters(filters);
    
    QStringList customSounds = customDir.entryList(QDir::Files);
    for (const QString& sound : customSounds) {
        QString fullPath = appSoundsDir + "/" + sound;
        
        // Basic validation at scan time
        QFileInfo info(fullPath);
        if (info.size() <= MAX_SOUND_FILE_SIZE) {
            soundFiles.append(fullPath);
        } else {
            qWarning() << "Skipping oversized sound file:" << fullPath 
                      << "(" << info.size() << " bytes)";
        }
    }
    
    return soundFiles;
}