using System;

namespace LuminaSharp;

// The script half of ObjectCore.h, imported into every script with a global using static so calls read as they do in C++.
public static class ObjectCore
{
    // Blocking load by virtual path, as LoadObject<T> in C++; the wrapper holds a strong reference like a TStrongObjectPtr member.
    public static T? LoadObject<T>(string Path) where T : NativeObject
    {
        IntPtr Pointer = Native.LoadObject(Path);
        return Pointer == IntPtr.Zero ? null : Wrapper<T>.ForObject(Pointer);
    }

    // As AsyncLoadObject in C++, the callback runs once on the game thread with the loaded object or null.
    public static void AsyncLoadObject<T>(string Path, Action<T?> Callback) where T : NativeObject
    {
        Native.LoadObjectAsync(Path, ScriptCallback.Bind(new ActionClosure(Pointer =>
            Callback(Pointer == 0 ? null : Wrapper<T>.ForObject((IntPtr)Pointer)))).Token);
    }
}
