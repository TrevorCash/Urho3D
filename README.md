![Urho3D logo](https://raw.githubusercontent.com/urho3d/Urho3D/master/bin/Data/Textures/LogoLarge.png)

# Urho3D Newton (forked from rebelfork)

[![Build Status](https://travis-ci.org/rokups/Urho3D.svg?branch=master)](https://travis-ci.org/rokups/Urho3D) [![Build status](https://ci.appveyor.com/api/projects/status/9b57do8manc0bfsq/branch/master?svg=true)](https://ci.appveyor.com/project/rokups/urho3d/branch/master) [![Join the chat at https://gitter.im/urho3d/Urho3D](https://badges.gitter.im/urho3d/Urho3D.svg)](https://gitter.im/urho3d/Urho3D?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

**Urho3D** is a free lightweight, cross-platform 2D and 3D game engine implemented in C++ and released under the MIT license. Greatly inspired by OGRE and Horde3D.

This repository is unofficial/personal fork of [urho3d.github.io](http://urho3d.github.io/).

## Differences
- This fork routinely merges from https://github.com/rokups/Urho3D.
- Newton Game Dynamics Integration (primary focus ongoing)
- Update rates are fixed and a prioritized as having constant timing.  If you want to you can ignore using P_TIMESTEP in your update logic.  But is still recommended you reference it.
- Update rates are seperated from render update rates. There are no "Frame" events. Application Logic should hook into more specific events like Update events.
- Tweeks Subsystem Provides Convience tools for saving and tweeking values in real time optionally via ImGui.
- Node internal source has been simplified
- Other smaller changes as well as fixes.

## Future Goals
- Seperate Engine Update and Rendering into 2 seperate threads as part of architecture.
- Replace Matrix3x4 with Matrix4x4 throughout the engine for simplification.
- Support Double Precision float by doing some typedef magic.
## License

Licensed under the MIT license, see [LICENSE](https://github.com/urho3d/Urho3D/blob/master/LICENSE) for details.

## Screenshots

![editor](https://user-images.githubusercontent.com/19151258/39086563-0c1fef1e-459c-11e8-86a8-9304e8af82d1.png)

## Dependencies

Urho3D bundles the following third-party libraries:
- Box2D 2.3.2 WIP (http://box2d.org)
- NewtonDynamics 3.14 (http://www.newtondynamics.com)
- Civetweb 1.7 (https://github.com/civetweb/civetweb)
- FreeType 2.8 (https://www.freetype.org)
- GLEW 1.13.0 (http://glew.sourceforge.net)
- kNet (https://github.com/juj/kNet)
- libcpuid 0.4.0+ (https://github.com/anrieff/libcpuid)
- LZ4 1.7.5 (https://github.com/lz4/lz4)
- MojoShader (https://icculus.org/mojoshader)
- Mustache 1.0 (https://mustache.github.io, https://github.com/kainjow/Mustache)
- Open Asset Import Library 4.1.0 (http://assimp.sourceforge.net)
- pugixml 1.7 (http://pugixml.org)
- rapidjson 1.1.0 (https://github.com/miloyip/rapidjson)
- Recast/Detour (https://github.com/memononen/recastnavigation)
- SDL 2.0.7 (https://www.libsdl.org)
- StanHull (https://codesuppository.blogspot.com/2006/03/john-ratcliffs-code-suppository-blog.html)
- stb_image 2.18 (https://nothings.org)
- stb_image_write 1.08 (https://nothings.org)
- stb_rect_pack 0.11 (https://nothings.org)
- stb_textedit 1.9 (https://nothings.org)
- stb_truetype 1.15 (https://nothings.org)
- stb_vorbis 1.13b (https://nothings.org)
- WebP (https://chromium.googlesource.com/webm/libwebp)
- imgui 1.64 (https://github.com/ocornut/imgui)
- tracy (https://bitbucket.org/wolfpld/tracy/)
- nativefiledialog (https://github.com/mlabbe/nativefiledialog)
- IconFontCppHeaders (https://github.com/juliettef/IconFontCppHeaders)
- ImGuizmo 1.61 (https://github.com/CedricGuillemet/ImGuizmo)
- deboost.context (https://github.com/septag/deboost.context)
- cr (https://github.com/fungos/cr)
- crunch (https://github.com/Unity-Technologies/crunch/)
- CLI11 v1.5.1 (https://github.com/CLIUtils/CLI11/)
- fmt 4.1.0 (http://fmtlib.net)
- spdlog 0.16.3 (https://github.com/gabime/spdlog)
- yaml-cpp 0.6.2 (https://github.com/jbeder/yaml-cpp/)

Urho3D optionally uses the following external third-party libraries:
- Mono (http://www.mono-project.com/download/stable/)
