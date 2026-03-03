dIanniX
=======
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/969e7860-9641-43fe-9673-bb4b82cc94f1" />


A modernized fork of IanniX with enhanced MIDI support and PipeWire compatibility.

**WARNING: This fork is currently under active development and not ready for production use. MIDI configuration with PipeWire may require additional system setup.**

About
-----

IanniX is a graphical open-source sequencer based on Iannis Xenakis works for digital art. This fork (dIanniX) extends the original project with modern MIDI infrastructure and improved Linux audio integration.

Changes from Original IanniX
-----------------------------

**MIDI System Modernization**
- Updated RtMidi library from 2012 version to 6.0.0
- Added ALSA MIDI API support for PipeWire compatibility
- MIDI virtual ports now work correctly with PipeWire-based DAWs (Bitwig Studio, Reaper, etc.)
- Added JACK MIDI API as fallback option
- Fixed MIDI note octave calculation (C4 now correctly maps to MIDI note 60)

**User Interface Improvements**
- Added comprehensive MIDI trigger settings in Inspector panel
  - MIDI channel selector (1-16 channels)
  - Note picker with full range (C-1 to G9, notes 0-127)
  - Velocity control (1-127)
  - Duration control (10-10000 ms)
  - Selective parameter application via checkboxes
- Added Delete key shortcut for removing selected objects
- Reduced excessive debug output for cleaner console

**Build System**
- Added CMakeLists.txt for modern CMake builds
- Organized build artifacts into build/ directory
- Created build.sh helper script with clean target
- Updated .gitignore for proper version control

Build Instructions
------------------

**Requirements**
- Qt5 development libraries
- ALSA development libraries (libasound2-dev)
- JACK development libraries (libjack-jackd2-dev, optional)

**Linux (qmake)**
```bash
./build.sh
```

**Linux (CMake)**
```bash
./build.sh cmake
```

**Clean build artifacts**
```bash
./build.sh clean
```

**Manual build with qmake**
```bash
qmake IanniX.pro
make -j$(nproc)
```

**Manual build with CMake**
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

PipeWire Configuration
----------------------

This fork uses ALSA MIDI API which provides full compatibility with PipeWire. When IanniX starts, it creates two virtual MIDI ports: "From IanniX" (output) and "To IanniX" (input). These ports are visible to all ALSA/PipeWire-compatible applications.

To verify MIDI ports are created:
```bash
aconnect -l
```

You can then connect these ports to your DAW or other MIDI applications using your DAW's MIDI routing panel, qpwgraph, or similar tools.

Original Documentation
----------------------

https://github.com/buzzinglight/IanniX/wiki

License
-------

See COPYING.txt for license information.
