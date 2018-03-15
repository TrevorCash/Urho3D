﻿using System;
using System.Runtime.InteropServices;

namespace Urho3D
{
    internal delegate void EventHandler(IntPtr gcHandle, uint type, IntPtr args);

    public partial class Object
    {
        public void SubscribeToEvent(StringHash eventType, Action<StringHash, VariantMap> function)
        {
            if (!ReferenceEquals(this, function.Target))
                throw new ArgumentException("Callback target object must be object on which SubscribeEvent() was " +
                                            "called.");
            Urho3D_Object_SubscribeToEvent(__ToPInvoke(this), GCHandle.ToIntPtr(GCHandle.Alloc(function)),
                eventType.Hash, (gcHandle, type, args) =>
                {
                    var callback = (Action<StringHash, VariantMap>) GCHandle.FromIntPtr(gcHandle).Target;
                    callback.Invoke(StringHash.__FromPInvoke(type), VariantMap.__FromPInvoke(args));
                }, IntPtr.Zero);
        }

        public void SubscribeToEvent(StringHash eventType, Action<VariantMap> function)
        {
            if (!ReferenceEquals(this, function.Target))
                throw new ArgumentException("Callback target object must be object on which SubscribeEvent() was " +
                                            "called.");
            Urho3D_Object_SubscribeToEvent(__ToPInvoke(this), GCHandle.ToIntPtr(GCHandle.Alloc(function)),
                eventType.Hash, (gcHandle, type, args) =>
                {
                    var callback = (Action<VariantMap>) GCHandle.FromIntPtr(gcHandle).Target;
                    callback.Invoke(VariantMap.__FromPInvoke(args));
                }, IntPtr.Zero);
        }

        public void SubscribeToEvent(Object sender, StringHash eventType, Action<StringHash, VariantMap> function)
        {
            if (!ReferenceEquals(this, function.Target))
                throw new ArgumentException("Callback target object must be object on which SubscribeEvent() was " +
                                            "called.");
            Urho3D_Object_SubscribeToEvent(__ToPInvoke(this), GCHandle.ToIntPtr(GCHandle.Alloc(function)),
                eventType.Hash, (gcHandle, type, args) =>
                {
                    var callback = (Action<StringHash, VariantMap>) GCHandle.FromIntPtr(gcHandle).Target;
                    callback.Invoke(StringHash.__FromPInvoke(type), VariantMap.__FromPInvoke(args));
                }, __ToPInvoke(sender));
        }

        public void SubscribeToEvent(Object sender, StringHash eventType, Action<VariantMap> function)
        {
            if (!ReferenceEquals(this, function.Target))
                throw new ArgumentException("Callback target object must be object on which SubscribeEvent() was " +
                                            "called.");
            Urho3D_Object_SubscribeToEvent(__ToPInvoke(this), GCHandle.ToIntPtr(GCHandle.Alloc(function)),
                eventType.Hash, (gcHandle, type, args) =>
                {
                    var callback = (Action<VariantMap>) GCHandle.FromIntPtr(gcHandle).Target;
                    callback.Invoke(VariantMap.__FromPInvoke(args));
                }, __ToPInvoke(sender));
        }

        [DllImport("Urho3DCSharp", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void Urho3D_Object_SubscribeToEvent(IntPtr receiver, IntPtr gcHandle, uint eventType,
            EventHandler function, IntPtr sender);
    }
}
