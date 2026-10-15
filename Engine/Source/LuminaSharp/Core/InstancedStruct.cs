using System;
using LuminaSharp;

namespace Lumina;

/// A view over a native FInstancedStruct, which owns a reflected struct value whose type is chosen at runtime.
[global::LuminaSharp.NativeSlotView]
public readonly unsafe struct FInstancedStruct
{
    private readonly nint Address;

    public FInstancedStruct(nint address)
    {
        Address = address;
    }

    /// False when the view has no storage, as opposed to holding no value.
    public bool IsBound => Address != 0;

    /// The stored struct's registered name, empty when nothing is stored.
    public string TypeName => IsBound ? Native.InstancedStructGetType(Address) : string.Empty;

    public bool IsValid => IsBound && Native.InstancedStructGetMemory(Address) != IntPtr.Zero;

    /// True when the stored value is T or derives from it.
    public bool Is<T>() where T : struct, INativeStruct<T>
        => IsBound && Native.InstancedStructIsA(Address, T.NativeName) != 0;

    // A handle on the stored value whose writes edit in place, throwing when it holds something other than a T.
    public T Get<T>() where T : struct, INativeStruct<T>
    {
        if (!TryGet(out T Value))
        {
            ThrowNotA(T.NativeName);
        }
        return Value;
    }

    // False with an empty handle when it holds something other than a T.
    public bool TryGet<T>(out T Value) where T : struct, INativeStruct<T>
    {
        nint Memory = Is<T>() ? Native.InstancedStructGetMemory(Address) : 0;
        Value = NativeStructs.FromAddress<T>(Memory);
        return Memory != 0;
    }

    [System.Diagnostics.CodeAnalysis.DoesNotReturn]
    private void ThrowNotA(string Expected)
    {
        string Held = TypeName;
        throw new InvalidOperationException($"Instanced struct holds {(Held.Length == 0 ? "nothing" : Held)}, not a {Expected}.");
    }

    /// Replaces the value with a fresh default-constructed T and returns the view of it.
    public T InitializeAs<T>() where T : struct, INativeStruct<T>
    {
        if (IsBound)
        {
            Native.InstancedStructInitializeAs(Address, T.NativeName);
        }
        return Get<T>();
    }

    /// Replaces the value with a fresh default-constructed instance of a runtime-named struct.
    public void InitializeAs(string StructName)
    {
        if (IsBound)
        {
            Native.InstancedStructInitializeAs(Address, StructName ?? string.Empty);
        }
    }

    /// Destroys the stored value, leaving the slot empty.
    public void Reset()
    {
        if (IsBound)
        {
            Native.InstancedStructReset(Address);
        }
    }
}

/// A view over a native TInstancedStruct, whose stored value always derives from TBase.
public readonly unsafe struct TInstancedStruct<TBase> where TBase : struct, INativeStruct<TBase>
{
    private readonly FInstancedStruct Inner;

    public TInstancedStruct(nint address)
    {
        Inner = new FInstancedStruct(address);
    }

    public bool IsBound => Inner.IsBound;

    public bool IsValid => Inner.IsValid;

    public string TypeName => Inner.TypeName;

    public TBase Get() => Inner.Get<TBase>();

    public bool TryGet(out TBase Value) => Inner.TryGet(out Value);

    public T Get<T>() where T : struct, INativeStruct<T> => Inner.Get<T>();

    public bool TryGet<T>(out T Value) where T : struct, INativeStruct<T> => Inner.TryGet(out Value);

    public bool Is<T>() where T : struct, INativeStruct<T> => Inner.Is<T>();

    public T InitializeAs<T>() where T : struct, INativeStruct<T> => Inner.InitializeAs<T>();

    public void InitializeAs(string StructName) => Inner.InitializeAs(StructName);

    public void Reset() => Inner.Reset();

    /// Drops the compile-time base, for code that handles any instanced struct.
    public FInstancedStruct AsUntyped() => Inner;
}
