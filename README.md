# Frite

**Frite** is a non-linear 2D animation software developped as part of the [MoStyle ANR project](https://benardp.github.io/mostyle/) by Melvin Even, Pierre Bénard and Pascal Barla. 

The animation system is described in the following publication:

[Non-linear Rough 2D Animation using Transient Embeddings.](https://inria.hal.science/hal-04006992) Melvin Even, Pierre Bénard, Pascal Barla. Computer Graphics Forum (Eurographics), Wiley, 2023.
 

It is originally based on [Pencil2D](https://www.pencil2d.org/).

**Note:** This is research software. As such, it may fail to run, crash, or otherwise not perform as expected. It is not intended for regular use in any kind of production pipeline.


## Dependencies
- [Eigen](https://eigen.tuxfamily.org/index.php?title=Main_Page)
- [CPD](https://github.com/gadomski/cpd)
- [FGT](https://github.com/gadomski/fgt)
- [nanoflann](https://github.com/jlblancoc/nanoflann)

They are automatically downloaded by `cmake` during the project configuration.

## Build instructions

1. Download and install **Qt 6** open source (https://www.qt.io/download)
2. Clone the repository and follow the build instructions depending on your OS:

### On Linux/macOS

Tested on Ubuntu 20.04 with GCC 9.3.0 and macOS (12.6) with Clang 14.

    cd frite
    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make

### On Windows

Tested on Windows 10 with Visual Studio 2019, `cmake-gui` and `vcpkg` (https://github.com/microsoft/vcpkg)

Packages to install with vcpkg:
* libjpeg-turbo:x64-windows
* libpng:x64-windows
* zlib:x64-windows

Build:

    cd frite
    mkdir build
    cd build
    cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<path to Qt cmake dir> -DCMAKE_TOOLCHAIN_FILE=<path to vcpkg cmake> -DVCPKG_TARGET_TRIPLET=x64-windows ..
    cmake --build . --config Release

Or open the Visual Studio solution.


## Multi-touch Wacom tablet on Ubuntu

For multi-touch inputs in Qton Ubuntu, "gestures" must be disabled. 
In the terminal, run the commands:

``` sh
xsetwacom --list devices
# trouver l'id correspondant au type TOUCH (ici 13)
xsetwacom --get 13 Gesture off
```

## Keyboard shortcuts

#### Tools

| Action               | Shortcut                                      |
|:-------------------- | ---------                                     |
| Draw                 | *P*                                           |
| Eraser               | *E*                                           |
| Pan                  | Hold *middle mouse button* or *H* to toggle   |
| Clear frame          | *K*                                           |
| Select group (lasso) | *S*                                           |
| Create group (lasso) | *G*                                           |
| Warp selected group  | *W*                                           |
| Trajectory  | *T*                                           |
| Spacing  | *I*                                           |

#### Actions

| Action                                         | Shortcut                |
|:---------------------------------------------- | ----------------------- |
| Toggle onion skin                              | *O*                     |
| Automatic registration *                       | *M*                     |
| Single-step registration *                     | *Ctrl+M*                     |
| Regularize lattice                             | *R*                     |
| Add breakdown                                  | *B*                     |
| Cross-fade (copy next KF strokes in pre group) | *C*                     |
| Copy the selected group into the next keyframe | *Shift+C*                     |
| Delete selected group                          | *Del*                   |
| Deselect group                                 | *Esc* or *Ctrl+Shift+A* |
| Deselect (in all KF of the current layer)      | *Shift+Esc*             |

\* For these actions holding *Ctrl* will use the entire next keyframe as the registration target. 

#### Timeline

|              Action              |    Shortcut               |
|----------------------------------|---------------------------|
| Play/pause                       | *Spacebar*                |
| Change frame                     | *Left/right arrow*        |
| Change keyframe                  | *Ctrl + left/right arrow* |

