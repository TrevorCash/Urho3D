//
// Copyright (c) 2008-2018 the Urho3D project.
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

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../Core/Profiler.h"
#include "../IO/Log.h"

#include "../Audio/Audio.h"
#include "../Engine/Engine.h"
#include "../Core/WorkQueue.h"
#if URHO3D_TASKS
#include "../Core/Tasks.h"
#endif
#if URHO3D_CSHARP
#include "../Script/ScriptSubsystem.h"
#endif
#include "../IO/FileSystem.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/Localization.h"
#if URHO3D_NETWORK
#include "../Network/Network.h"
#endif
#include "../Input/Input.h"
#include "../UI/UI.h"
#if URHO3D_SYSTEMUI
#include "../SystemUI/SystemUI.h"
#endif

#ifndef MINI_URHO
#include <SDL/SDL.h>
#ifdef URHO3D_IK
#include <ik/log.h>
#include <ik/memory.h>
#endif
#endif

#include "../DebugNew.h"

namespace Urho3D
{

#ifndef MINI_URHO
// Keeps track of how many times SDL was initialised so we know when to call SDL_Quit().
static int sdlInitCounter = 0;

// Keeps track of how many times IK was initialised
static int ikInitCounter = 0;

// Reroute all messages from the ik library to the Urho3D log
static void HandleIKLog(const char* msg)
{
    URHO3D_LOGINFOF("[IK] %s", msg);
}
#endif

void EventReceiverGroup::BeginSendEvent()
{
    ++inSend_;
}

void EventReceiverGroup::EndSendEvent()
{
    assert(inSend_ > 0);
    --inSend_;

    if (inSend_ == 0 && dirty_)
    {
        /// \todo Could be optimized by erase-swap, but this keeps the receiver order
        for (unsigned i = receivers_.Size() - 1; i < receivers_.Size(); --i)
        {
            if (!receivers_[i])
                receivers_.Erase(i);
        }

        dirty_ = false;
    }
}

void EventReceiverGroup::Add(Object* object)
{
    if (object)
        receivers_.Push(object);
}

void EventReceiverGroup::Remove(Object* object)
{
    if (inSend_ > 0)
    {
        PODVector<Object*>::Iterator i = receivers_.Find(object);
        if (i != receivers_.End())
        {
            (*i) = nullptr;
            dirty_ = true;
        }
    }
    else
        receivers_.Remove(object);
}

void RemoveNamedAttribute(HashMap<StringHash, Vector<AttributeInfo> >& attributes, StringHash objectType, const char* name)
{
    HashMap<StringHash, Vector<AttributeInfo> >::Iterator i = attributes.Find(objectType);
    if (i == attributes.End())
        return;

    Vector<AttributeInfo>& infos = i->second_;

    for (Vector<AttributeInfo>::Iterator j = infos.Begin(); j != infos.End(); ++j)
    {
        if (!j->name_.Compare(name, true))
        {
            infos.Erase(j);
            break;
        }
    }

    // If the vector became empty, erase the object type from the map
    if (infos.Empty())
        attributes.Erase(i);
}

Context::Context() :
    eventHandler_(nullptr)
{
#ifdef __ANDROID__
    // Always reset the random seed on Android, as the Urho3D library might not be unloaded between runs
    SetRandomSeed(1);
#endif

    // Set the main thread ID (assuming the Context is created in it)
    Thread::SetMainThread();
}

Context::~Context()
{
    // Remove subsystems that use SDL in reverse order of construction, so that Graphics can shut down SDL last
    /// \todo Context should not need to know about subsystems
    RemoveSubsystem("Audio");
    RemoveSubsystem("UI");
    RemoveSubsystem("Input");
    RemoveSubsystem("Renderer");
    RemoveSubsystem("Graphics");

    subsystems_.Clear();
    factories_.Clear();

    // Delete allocated event data maps
    for (PODVector<VariantMap*>::Iterator i = eventDataMaps_.Begin(); i != eventDataMaps_.End(); ++i)
        delete *i;
    eventDataMaps_.Clear();
}

SharedPtr<Object> Context::CreateObject(StringHash objectType)
{
    HashMap<StringHash, SharedPtr<ObjectFactory> >::ConstIterator i = factories_.Find(objectType);
    if (i != factories_.End())
        return i->second_->CreateObject();
    else
        return SharedPtr<Object>();
}

bool Context::RegisterFactory(ObjectFactory* factory)
{
    if (!factory)
        return false;

    factories_[factory->GetType()] = factory;
	return true;
}

bool Context::RegisterFactory(ObjectFactory* factory, const char* category)
{
    if (!factory)
        return false;

	if (RegisterFactory(factory))
	{
		if (String::CStringLength(category))
			objectCategories_[category].Push(factory->GetType());
		return true;
	}
	return false;
}

void Context::RemoveFactory(StringHash type)
{
    factories_.Erase(type);
}

void Context::RemoveFactory(StringHash type, const char* category)
{
    RemoveFactory(type);
    if (String::CStringLength(category))
        objectCategories_[category].Remove(type);
}

void Context::RemoveFactories(const char* category)
{
    if (objectCategories_.Contains(category))
    {
        Vector<StringHash> typesInCategory = objectCategories_[category];
        for (StringHash type : typesInCategory) {
            RemoveFactory(type);
        }
        objectCategories_.Erase(category);
    }
}


void Context::RegisterSubsystem(Object* object)
{
    if (!object)
        return;

    subsystems_[object->GetType()] = object;
}

void Context::RemoveSubsystem(StringHash objectType)
{
    HashMap<StringHash, SharedPtr<Object> >::Iterator i = subsystems_.Find(objectType);
    if (i != subsystems_.End())
        subsystems_.Erase(i);
}

AttributeHandle Context::RegisterAttribute(StringHash objectType, const AttributeInfo& attr)
{
    // None or pointer types can not be supported
    if (attr.type_ == VAR_NONE || attr.type_ == VAR_VOIDPTR || attr.type_ == VAR_PTR
        || attr.type_ == VAR_CUSTOM_HEAP || attr.type_ == VAR_CUSTOM_STACK)
    {
        URHO3D_LOGWARNING("Attempt to register unsupported attribute type " + Variant::GetTypeName(attr.type_) + " to class " +
            GetTypeName(objectType));
        return AttributeHandle();
    }

    AttributeHandle handle;

    Vector<AttributeInfo>& objectAttributes = attributes_[objectType];
    objectAttributes.Push(attr);
    handle.attributeInfo_ = &objectAttributes.Back();

    if (attr.mode_ & AM_NET)
    {
        Vector<AttributeInfo>& objectNetworkAttributes = networkAttributes_[objectType];
        objectNetworkAttributes.Push(attr);
        handle.networkAttributeInfo_ = &objectNetworkAttributes.Back();
    }
    return handle;
}

void Context::RemoveAttribute(StringHash objectType, const char* name)
{
    RemoveNamedAttribute(attributes_, objectType, name);
    RemoveNamedAttribute(networkAttributes_, objectType, name);
}

void Context::RemoveAllAttributes(StringHash objectType)
{
    attributes_.Erase(objectType);
    networkAttributes_.Erase(objectType);
}

void Context::UpdateAttributeDefaultValue(StringHash objectType, const char* name, const Variant& defaultValue)
{
    AttributeInfo* info = GetAttribute(objectType, name);
    if (info)
        info->defaultValue_ = defaultValue;
}

VariantMap& Context::GetEventDataMap()
{
    unsigned nestingLevel = eventSenders_.Size();
    while (eventDataMaps_.Size() < nestingLevel + 1)
        eventDataMaps_.Push(new VariantMap());

    VariantMap& ret = *eventDataMaps_[nestingLevel];
    ret.Clear();
    return ret;
}

#ifndef MINI_URHO
bool Context::RequireSDL(unsigned int sdlFlags)
{
    // Always increment, the caller must match with ReleaseSDL(), regardless of
    // what happens.
    ++sdlInitCounter;

    // Need to call SDL_Init() at least once before SDL_InitSubsystem()
    if (sdlInitCounter == 1)
    {
        URHO3D_LOGDEBUG("Initialising SDL");
        if (SDL_Init(0) != 0)
        {
            URHO3D_LOGERRORF("Failed to initialise SDL: %s", SDL_GetError());
            return false;
        }
    }

    Uint32 remainingFlags = sdlFlags & ~SDL_WasInit(0);
    if (remainingFlags != 0)
    {
        if (SDL_InitSubSystem(remainingFlags) != 0)
        {
            URHO3D_LOGERRORF("Failed to initialise SDL subsystem: %s", SDL_GetError());
            return false;
        }
    }

    return true;
}

void Context::ReleaseSDL()
{
    --sdlInitCounter;

    if (sdlInitCounter == 0)
    {
        URHO3D_LOGDEBUG("Quitting SDL");
        SDL_QuitSubSystem(SDL_INIT_EVERYTHING);
        SDL_Quit();
    }

    if (sdlInitCounter < 0)
        URHO3D_LOGERROR("Too many calls to Context::ReleaseSDL()!");
}

#ifdef URHO3D_IK
void Context::RequireIK()
{
    // Always increment, the caller must match with ReleaseSDL(), regardless of
    // what happens.
    ++ikInitCounter;

    if (ikInitCounter == 1)
    {
        URHO3D_LOGDEBUG("Initialising Inverse Kinematics library");
        ik_memory_init();
        ik_log_init(IK_LOG_NONE);
        ik_log_register_listener(HandleIKLog);
    }
}

void Context::ReleaseIK()
{
    --ikInitCounter;

    if (ikInitCounter == 0)
    {
        URHO3D_LOGDEBUG("De-initialising Inverse Kinematics library");
        ik_log_unregister_listener(HandleIKLog);
        ik_log_deinit();
        ik_memory_deinit();
    }

    if (ikInitCounter < 0)
        URHO3D_LOGERROR("Too many calls to Context::ReleaseIK()");
}
#endif // ifdef URHO3D_IK
#endif // ifndef MINI_URHO

void Context::CopyBaseAttributes(StringHash baseType, StringHash derivedType)
{
    // Prevent endless loop if mistakenly copying attributes from same class as derived
    if (baseType == derivedType)
    {
        URHO3D_LOGWARNING("Attempt to copy base attributes to itself for class " + GetTypeName(baseType));
        return;
    }

    const Vector<AttributeInfo>* baseAttributes = GetAttributes(baseType);
    if (baseAttributes)
    {
        for (unsigned i = 0; i < baseAttributes->Size(); ++i)
        {
            const AttributeInfo& attr = baseAttributes->At(i);
            attributes_[derivedType].Push(attr);
            if (attr.mode_ & AM_NET)
                networkAttributes_[derivedType].Push(attr);
        }
    }
}

Object* Context::GetSubsystem(StringHash type) const
{
    HashMap<StringHash, SharedPtr<Object> >::ConstIterator i = subsystems_.Find(type);
    if (i != subsystems_.End())
        return i->second_;
    else
        return nullptr;
}

const Variant& Context::GetGlobalVar(StringHash key) const
{
    VariantMap::ConstIterator i = globalVars_.Find(key);
    return i != globalVars_.End() ? i->second_ : Variant::EMPTY;
}

void Context::SetGlobalVar(StringHash key, const Variant& value)
{
    globalVars_[key] = value;
}

Object* Context::GetEventSender() const
{
    if (!eventSenders_.Empty())
        return eventSenders_.Back();
    else
        return nullptr;
}

const String& Context::GetTypeName(StringHash objectType) const
{
    // Search factories to find the hash-to-name mapping
    HashMap<StringHash, SharedPtr<ObjectFactory> >::ConstIterator i = factories_.Find(objectType);
    return i != factories_.End() ? i->second_->GetTypeName() : String::EMPTY;
}

AttributeInfo* Context::GetAttribute(StringHash objectType, const char* name)
{
    HashMap<StringHash, Vector<AttributeInfo> >::Iterator i = attributes_.Find(objectType);
    if (i == attributes_.End())
        return nullptr;

    Vector<AttributeInfo>& infos = i->second_;

    for (Vector<AttributeInfo>::Iterator j = infos.Begin(); j != infos.End(); ++j)
    {
        if (!j->name_.Compare(name, true))
            return &(*j);
    }

    return nullptr;
}

void Context::AddEventReceiver(Object* receiver, StringHash eventType)
{
    SharedPtr<EventReceiverGroup>& group = eventReceivers_[eventType];
    if (!group)
        group = new EventReceiverGroup();
    group->Add(receiver);
}

void Context::AddEventReceiver(Object* receiver, Object* sender, StringHash eventType)
{
    SharedPtr<EventReceiverGroup>& group = specificEventReceivers_[sender][eventType];
    if (!group)
        group = new EventReceiverGroup();
    group->Add(receiver);
}

void Context::RemoveEventSender(Object* sender)
{
    HashMap<Object*, HashMap<StringHash, SharedPtr<EventReceiverGroup> > >::Iterator i = specificEventReceivers_.Find(sender);
    if (i != specificEventReceivers_.End())
    {
        for (HashMap<StringHash, SharedPtr<EventReceiverGroup> >::Iterator j = i->second_.Begin(); j != i->second_.End(); ++j)
        {
            for (PODVector<Object*>::Iterator k = j->second_->receivers_.Begin(); k != j->second_->receivers_.End(); ++k)
            {
                Object* receiver = *k;
                if (receiver)
                    receiver->RemoveEventSender(sender);
            }
        }
        specificEventReceivers_.Erase(i);
    }
}

void Context::RemoveEventReceiver(Object* receiver, StringHash eventType)
{
    EventReceiverGroup* group = GetEventReceivers(eventType);
    if (group)
        group->Remove(receiver);
}

void Context::RemoveEventReceiver(Object* receiver, Object* sender, StringHash eventType)
{
    EventReceiverGroup* group = GetEventReceivers(sender, eventType);
    if (group)
        group->Remove(receiver);
}

void Context::BeginSendEvent(Object* sender, StringHash eventType)
{
    eventSenders_.Push(sender);
}

void Context::EndSendEvent()
{
    eventSenders_.Pop();
}

void Context::RegisterSubsystem(Engine* subsystem)
{
    engine_ = subsystem;
    RegisterSubsystem((Object*)subsystem);
}

void Context::RegisterSubsystem(Time* subsystem)
{
    time_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}

void Context::RegisterSubsystem(WorkQueue* subsystem)
{
    workQueue_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#if URHO3D_PROFILING
void Context::RegisterSubsystem(Profiler* subsystem)
{
    profiler_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#endif
void Context::RegisterSubsystem(FileSystem* subsystem)
{
    fileSystem_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#if URHO3D_LOGGING
void Context::RegisterSubsystem(Log* subsystem)
{
    log_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#endif
void Context::RegisterSubsystem(ResourceCache* subsystem)
{
    cache_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}

void Context::RegisterSubsystem(Localization* subsystem)
{
    l18n_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#if URHO3D_NETWORK
void Context::RegisterSubsystem(Network* subsystem)
{
    network_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#endif
void Context::RegisterSubsystem(Input* subsystem)
{
    input_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}

void Context::RegisterSubsystem(Audio* subsystem)
{
    audio_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}

void Context::RegisterSubsystem(UI* subsystem)
{
    ui_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#if URHO3D_TASKS
void Context::RegisterSubsystem(Tasks* subsystem)
{
    tasks_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#endif
#if URHO3D_CSHARP
void Context::RegisterSubsystem(ScriptSubsystem* subsystem)
{
    scripts_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#endif
#if URHO3D_SYSTEMUI
void Context::RegisterSubsystem(SystemUI* subsystem)
{
    systemUi_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#endif
}
