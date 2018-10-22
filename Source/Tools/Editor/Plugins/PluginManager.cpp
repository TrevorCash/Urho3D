//
// Copyright (c) 2018 Rokas Kupstys
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#if URHO3D_PLUGINS

#define CR_HOST

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/Thread.h>
#include <Urho3D/Engine/PluginApplication.h>
#include <Urho3D/IO/File.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Script/Script.h>
#include <EditorEvents.h>
#include "Editor.h"
#include "Plugins/PluginManager.h"

namespace Urho3D
{

URHO3D_EVENT(E_ENDFRAMEPRIVATE, EndFramePrivate)
{
}

#if __linux__
static const char* platformDynamicLibrarySuffix = ".so";
#elif _WIN32
static const char* platformDynamicLibrarySuffix = ".dll";
#elif __APPLE__
static const char* platformDynamicLibrarySuffix = ".dylib";
#else
#   error Unsupported platform.
#endif

#if URHO3D_CSHARP && URHO3D_PLUGINS
extern "C" URHO3D_EXPORT_API void ParseArgumentsC(int argc, char** argv) { ParseArguments(argc, argv); }
extern "C" URHO3D_EXPORT_API Application* CreateEditorApplication(Context* context) { return new Editor(context); }
#endif

Plugin::Plugin(Context* context)
    : Object(context)
{
}

bool Plugin::Unload()
{
#if URHO3D_PLUGINS
    if (type_ == PLUGIN_NATIVE)
    {
        cr_plugin_close(nativeContext_);
        nativeContext_.userdata = nullptr;
        return true;
    }
#if URHO3D_CSHARP
    else if (type_ == PLUGIN_MANAGED)
    {
        Script* script = GetSubsystem<Script>();
        script->UnloadRuntime();        // Destroy plugin AppDomain.
        script->LoadRuntime();          // Create new empty plugin AppDomain. Caller is responsible for plugin reinitialization.
        return true;
    }
#endif
#endif
    return false;
}

PluginManager::PluginManager(Context* context)
    : Object(context)
{
#if URHO3D_PLUGINS
    CleanUp();
    SubscribeToEvent(E_ENDFRAMEPRIVATE, [this](StringHash, VariantMap&) { OnEndFrame(); });
    SubscribeToEvent(E_SIMULATIONSTART, [this](StringHash, VariantMap&) {
        for (auto& plugin : plugins_)
        {
            if (plugin->nativeContext_.userdata != nullptr && plugin->nativeContext_.userdata != context_)
                reinterpret_cast<PluginApplication*>(plugin->nativeContext_.userdata)->Start();
        }
    });
    SubscribeToEvent(E_SIMULATIONSTOP, [this](StringHash, VariantMap&) {
        for (auto& plugin : plugins_)
        {
            if (plugin->nativeContext_.userdata != nullptr && plugin->nativeContext_.userdata != context_)
                reinterpret_cast<PluginApplication*>(plugin->nativeContext_.userdata)->Stop();
        }
    });
#if URHO3D_CSHARP
    // Initialize AppDomain for managed plugins.
    GetSubsystem<Script>()->LoadRuntime();
#endif
#endif
}

Plugin* PluginManager::Load(const String& name)
{
#if URHO3D_PLUGINS
    if (Plugin* loaded = GetPlugin(name))
        return loaded;

    CleanUp();

    String pluginPath = NameToPath(name);
    if (pluginPath.Empty())
        return nullptr;

    SharedPtr<Plugin> plugin(new Plugin(context_));
    plugin->type_ = GetPluginType(context_, pluginPath);

    if (plugin->type_ == PLUGIN_NATIVE)
    {
        if (cr_plugin_load(plugin->nativeContext_, pluginPath.CString()))
        {
            plugin->nativeContext_.userdata = context_;
            plugin->name_ = name;
            plugin->path_ = pluginPath;
            plugin->mtime_ = GetFileSystem()->GetLastModifiedTime(pluginPath);
            plugins_.Push(plugin);
            return plugin.Get();
        }
        else
            URHO3D_LOGWARNINGF("Failed loading native plugin \"%s\".", name.CString());
    }
#if URHO3D_CSHARP
    else if (plugin->type_ == PLUGIN_MANAGED)
    {
        if (GetSubsystem<Script>()->LoadAssembly(pluginPath))
        {
            plugin->name_ = name;
            plugin->path_ = pluginPath;
            plugin->mtime_ = GetFileSystem()->GetLastModifiedTime(pluginPath);
            plugins_.Push(plugin);
            return plugin.Get();
        }
    }
#endif
#endif
    return nullptr;
}

void PluginManager::Unload(Plugin* plugin)
{
    if (plugin == nullptr)
        return;

    auto it = plugins_.Find(SharedPtr<Plugin>(plugin));
    if (it == plugins_.End())
    {
        URHO3D_LOGERRORF("Plugin %s was never loaded.", plugin->name_.CString());
        return;
    }

    plugin->unloading_ = true;
}

void PluginManager::OnEndFrame()
{
#if URHO3D_PLUGINS
#if URHO3D_CSHARP
    Script* script = GetSubsystem<Script>();
    // C# plugin auto-reloading.
    bool reloadManagedRuntime = false;
    for (auto it = plugins_.Begin(); it != plugins_.End() && !reloadManagedRuntime; it++)
    {
        Plugin* plugin = it->Get();
        reloadManagedRuntime = plugin->type_ == PLUGIN_MANAGED &&
                               plugin->mtime_ < GetFileSystem()->GetLastModifiedTime(plugin->path_);
    }

    if (reloadManagedRuntime)
    {
        script->UnloadRuntime();
        script->LoadRuntime();
        for (auto& plugin : plugins_)
        {
            if (plugin->type_ == PLUGIN_MANAGED)
            {
                plugin->mtime_ = GetFileSystem()->GetLastModifiedTime(plugin->path_);
                script->LoadAssembly(plugin->path_);
            }
        }
        URHO3D_LOGINFO("Managed plugins were reloaded.");
    }
#endif

    bool eventSent = false;
    for (auto it = plugins_.Begin(); it != plugins_.End();)
    {
        Plugin* plugin = it->Get();

        if (plugin->unloading_)
        {
            if (!eventSent)
            {
                SendEvent(E_EDITORUSERCODERELOADSTART);
                eventSent = true;
            }
            // Actual unload
            plugin->Unload();
#if URHO3D_CSHARP
            if (plugin->type_ == PLUGIN_MANAGED)
            {
                // Now load back all managed plugins except this one.
                for (auto& plug : plugins_)
                {
                    if (plug == plugin || plug->type_ == PLUGIN_NATIVE)
                        continue;
                    script->LoadAssembly(plug->path_);
                }
            }
#endif
            URHO3D_LOGINFOF("Plugin %s was unloaded.", plugin->name_.CString());
            it = plugins_.Erase(it);
        }
        else if (plugin->type_ == PLUGIN_NATIVE && plugin->nativeContext_.userdata)
        {
            bool reloading = cr_plugin_changed(plugin->nativeContext_);
            if (reloading)
            {
                if (!eventSent)
                {
                    SendEvent(E_EDITORUSERCODERELOADSTART);
                    eventSent = true;
                }
            }

            if (cr_plugin_update(plugin->nativeContext_) != 0)
            {
                URHO3D_LOGERRORF("Processing plugin \"%s\" failed and it was unloaded.",
                    GetFileNameAndExtension(plugin->name_).CString());
                cr_plugin_close(plugin->nativeContext_);
                plugin->nativeContext_.userdata = nullptr;
                continue;
            }

            if (reloading)
            {
                if (plugin->nativeContext_.userdata != nullptr)
                {
                    URHO3D_LOGINFOF("Loaded plugin \"%s\" version %d.",
                        GetFileNameAndExtension(plugin->name_).CString(), plugin->nativeContext_.version);
                }
            }

            it++;
        }
        else
            it++;
    }

    if (eventSent)
        SendEvent(E_EDITORUSERCODERELOADEND);
#endif
}

void PluginManager::CleanUp(String directory)
{
#if URHO3D_PLUGINS
    if (directory.Empty())
        directory = GetFileSystem()->GetProgramDir();

    if (!GetFileSystem()->DirExists(directory))
        return;

    StringVector files;
    GetFileSystem()->ScanDir(files, directory, "*.*", SCAN_FILES, false);

    for (const String& fileName : files)
    {
        String filePath = directory + fileName;
        String baseName = GetFileName(fileName);
        if (IsDigit(static_cast<unsigned int>(baseName.Back())) && GetPluginType(context_, filePath) != PLUGIN_INVALID)
        {
            GetFileSystem()->Delete(filePath);
            if (filePath.EndsWith(".dll"))
                GetFileSystem()->Delete(ReplaceExtension(filePath, ".pdb"));
        }
    }
#endif
}

Plugin* PluginManager::GetPlugin(const String& name)
{
    for (auto it = plugins_.Begin(); it != plugins_.End(); it++)
    {
        if (it->Get()->name_ == name)
            return it->Get();
    }
    return nullptr;
}

String PluginManager::NameToPath(const String& name) const
{
    FileSystem* fs = GetFileSystem();
    String result;

#if __linux__ || __APPLE__
    result = ToString("%slib%s%s", fs->GetProgramDir().CString(), name.CString(), platformDynamicLibrarySuffix);
    if (fs->FileExists(result))
        return result;
#endif

#if !_WIN32
    result = ToString("%s%s%s", fs->GetProgramDir().CString(), name.CString(), ".dll");
    if (fs->FileExists(result))
        return result;
#endif

    result = ToString("%s%s%s", fs->GetProgramDir().CString(), name.CString(), platformDynamicLibrarySuffix);
    if (fs->FileExists(result))
        return result;

    return String::EMPTY;
}

String PluginManager::PathToName(const String& path)
{
#if !_WIN32
    if (path.EndsWith(platformDynamicLibrarySuffix))
    {
        String name = GetFileName(path);
#if __linux__ || __APPLE__
        if (name.StartsWith("lib"))
            name = name.Substring(3);
#endif
        return name;
    }
    else
#endif
    if (path.EndsWith(".dll"))
        return GetFileName(path);
    return String::EMPTY;
}

PluginManager::~PluginManager()
{
    for (auto& plugin : plugins_)
    {
        if (plugin->type_ == PLUGIN_NATIVE)
            // Native plugins can be unloaded one by one.
            plugin->Unload();
    }

#if URHO3D_CSHARP
    // Managed plugins can not be unloaded one at a time. Entire plugin AppDomain must be dropped.
    GetSubsystem<Script>()->UnloadRuntime();
#endif
}

}

#endif
