# Dark File v1.0 (Beta Version)

![Version](https://img.shields.io/badge/version-1.0-blue.svg?style=flat-square)
![License](https://img.shields.io/badge/license-MIT-green.svg?style=flat-square)
![Language](https://img.shields.io/badge/language-C-orange.svg?style=flat-square)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20Android%20%7C%20macOS-lightgrey?style=flat-square)

**Dark File** is a high-performance, multi-threaded file management utility designed for maximum speed and efficiency. Written in pure C, it leverages **Zero-Copy (`sendfile`)** technology and intelligent thread pooling to handle massive file operations significantly faster than traditional system tools.

---

## 🚀 Key Features

- ⚡ **4-Tier Performance Engine**  
  Choose from **Lower**, **Standard**, **High**, or **Ultra** modes to match your hardware capability.

- 🧠 **Auto-Hardware Detection**  
  Automatically detects CPU core count to optimize thread usage.

- 📂 **Smart Organizer**  
  Sorts thousands of files into categorized folders:
  - Images
  - Videos
  - Documents
  - Archives
  - APKs

- 🛡️ **Zero-Copy Optimization**  
  Uses kernel-level `sendfile()` on Linux and Android for minimal CPU overhead, with automatic fallback on unsupported platforms.

- 🔧 **Smart Path Logic**  
  Mimics `cp` behavior and supports recursive directory creation (`mkdir -p`).

- ✅ **Cross-Platform Support**  
  Runs on:
  - Linux
  - Android (Termux)
  - macOS
  - Windows (MinGW / WSL)

---

## 📥 Installation

### Option 1: Quick Install (Recommended)

**Linux / Android (Termux) / macOS:**
Run the automated installer script directly from your terminal:

```bash
git clone https://github.com/darkscriptstudio/DarkFile.git
cd DarkFile
chmod +x install.sh
./install.sh
```

### Option 2: Download Precompiled Binaries (Recommended)

Download the appropriate binary from the [**Releases**](https://github.com/darkscriptstudio/DarkFile/releases) page:

- `darkfile-linux` — Linux
- `darkfile-termux` — Android (Termux)
- `darkfile.exe` — Windows

---

### Option 3: Manual Build from Source

If you want to manually compile the tool, follow these steps:

1. **Clone the Repository:** <br> ```git clone https://github.com/darkscriptstudio/DarkFile.git cd DarkFile```
2. **Compile:** <br> **Linux / Android (Termux) / macOS:** <br> ``` gcc src/darkfile.c -o darkfile -pthread -O3 ``` <br><br> **Windows (MinGW):** <br> ``` gcc src/darkfile.c -o darkfile.exe -pthread -O3 ```




## 📖 Usage Guide

Run the tool from your terminal.

### Syntax

```bash
./darkfile <FLAG> <SOURCE> [DESTINATION]
```

## 📌 Available Commands

| Flag       | Action    | Description                                                     |
|------------|-----------|-----------------------------------------------------------------|
| `-c`       | Copy      | Multi-threaded file copy with smart path handling               |
| `-m`       | Move      | Moves files and removes empty source directories                |
| `-d`       | Delete    | Fast, parallel deletion of directories                          |
| `-o`       | Organize  | Sorts files into categorized folders                            |
| `--count`  | Count     | Counts files and calculates total size                          |

## 🧪 Examples

### High-Speed Copy (Smart Path)

Copies the Downloads folder into Backup.

```bash
./darkfile -c /sdcard/Downloads /sdcard/Backup/
```

### Organize a Messy Folder

Sorts all files in Downloads into subfolders (Images, Videos, etc.).

```bash
./darkfile -o /sdcard/Downloads
```

### Bulk Deletion

Rapidly deletes a heavy directory.

```bash
./darkfile -d /sdcard/Old_Junk_Files
```

## ⚙️ Performance Profiles

During execution, select one of the following modes:

- **Lower (1 Thread)** <br> Safe for background use
- **Standard (Recommended)** <br> Uses CPU core count for balanced performance
- **High** <br> Uses 2× CPU core count for aggressive speed
- **Ultra** <br> Uses 4× CPU core count

> ⚠️ May cause system lag on low-end devices

## 🤝 Contributing

Contributions are welcome!

1. Fork the repository
2. Create a feature branch <br> ``` git checkout -b feature-name```
3. Commit your changes <br> ``` git commit -m "Add new feature" ```
4. Push to the branch <br> ``` git push origin feature-name ```
5. Open a Pull Request

## 📜 License

**Copyright (c) 2026 Dark Script Studio**

This project is licensed under the MIT License.
You are free to use, modify, and distribute this software provided the original copyright notice and license are included.
See the LICENSE file for details.
