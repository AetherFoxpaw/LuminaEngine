using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace LuminaSharp;

// Entities carrying any of these are left out of a view, mirroring ECS::TExclude.
public readonly struct Exclude
{
    internal readonly IntPtr[]? Tokens;

    private Exclude(IntPtr[] Tokens)
    {
        this.Tokens = Tokens;
    }

    public static Exclude None => default;

    public static Exclude Of<T1>() where T1 : struct, IComponent<T1> => new(ExcludeSet<T1>.Tokens);

    public static Exclude Of<T1, T2>() where T1 : struct, IComponent<T1> where T2 : struct, IComponent<T2>
        => new(ExcludeSet<T1, T2>.Tokens);

    public static Exclude Of<T1, T2, T3>() where T1 : struct, IComponent<T1> where T2 : struct, IComponent<T2> where T3 : struct, IComponent<T3>
        => new(ExcludeSet<T1, T2, T3>.Tokens);

    public static Exclude Of<T1, T2, T3, T4>() where T1 : struct, IComponent<T1> where T2 : struct, IComponent<T2> where T3 : struct, IComponent<T3> where T4 : struct, IComponent<T4>
        => new(ExcludeSet<T1, T2, T3, T4>.Tokens);

    private static class ExcludeSet<T1> where T1 : struct, IComponent<T1>
    {
        public static readonly IntPtr[] Tokens = { ComponentOps<T1>.Token };
    }

    private static class ExcludeSet<T1, T2> where T1 : struct, IComponent<T1> where T2 : struct, IComponent<T2>
    {
        public static readonly IntPtr[] Tokens = { ComponentOps<T1>.Token, ComponentOps<T2>.Token };
    }

    private static class ExcludeSet<T1, T2, T3> where T1 : struct, IComponent<T1> where T2 : struct, IComponent<T2> where T3 : struct, IComponent<T3>
    {
        public static readonly IntPtr[] Tokens = { ComponentOps<T1>.Token, ComponentOps<T2>.Token, ComponentOps<T3>.Token };
    }

    private static class ExcludeSet<T1, T2, T3, T4> where T1 : struct, IComponent<T1> where T2 : struct, IComponent<T2> where T3 : struct, IComponent<T3> where T4 : struct, IComponent<T4>
    {
        public static readonly IntPtr[] Tokens = { ComponentOps<T1>.Token, ComponentOps<T2>.Token, ComponentOps<T3>.Token, ComponentOps<T4>.Token };
    }
}

// Walks a native view one chunk per crossing, handing out tracked handles so one kept past the loop stays safe.
internal unsafe struct ViewCursor : IDisposable
{
    private const int ChunkSize = 1024;
    private const int MaxIncludes = 8;

    private readonly ulong World;
    private readonly int Includes;
    private fixed long Tokens[MaxIncludes];
    private fixed long EpochSlots[MaxIncludes];
    private uint* Entities;
    private IntPtr* Pointers;
    private IntPtr State;
    private int Count;
    private int Index;

    public ViewCursor(ulong World, ReadOnlySpan<IntPtr> IncludeTokens, Exclude Filter)
    {
        this.World = World;
        Includes = IncludeTokens.Length;
        Entities = (uint*)NativeMemory.Alloc((nuint)(ChunkSize * sizeof(uint) + MaxIncludes * sizeof(NativeRef)));
        Pointers = (IntPtr*)NativeMemory.Alloc((nuint)(ChunkSize * Includes) * (nuint)sizeof(IntPtr));
        Count = 0;
        Index = -1;

        IntPtr* Include = stackalloc IntPtr[MaxIncludes];
        for (int i = 0; i < Includes; ++i)
        {
            Tokens[i] = IncludeTokens[i];
            Include[i] = IncludeTokens[i];
        }

        IntPtr[]? Excluded = Filter.Tokens;
        fixed (IntPtr* Exclude = Excluded)
        {
            State = Native.ViewBegin(World, Include, Includes, Exclude, Excluded?.Length ?? 0);
        }

        uint** Slots = stackalloc uint*[MaxIncludes];
        if (State != IntPtr.Zero)
        {
            Native.ViewIncludeEpochs(State, Slots, Includes);
        }
        for (int i = 0; i < Includes; ++i)
        {
            EpochSlots[i] = State != IntPtr.Zero ? (long)Slots[i] : 0;
        }
    }

    public bool MoveNext()
    {
        if (State == IntPtr.Zero)
        {
            return false;
        }
        if (++Index < Count)
        {
            return true;
        }
        Count = Native.ViewNextChunk(State, Entities, Pointers, ChunkSize, Includes);
        Index = 0;
        return Count > 0;
    }

    public FEntity Entity => new(Entities[Index]);

    public T Get<T>(int Include) where T : struct, INativeStruct
    {
        uint EntityId = Entities[Index];
        IntPtr Pointer = Pointers[Index * Includes + Include];
        return NativeStructs.FromRef<T>(new NativeRef(Pointer, World, EntityId, (IntPtr)Tokens[Include], (uint*)EpochSlots[Include]));
    }

    // One handle per include past the entity ids, which the ref-returning enumerators hand out.
    public void* Slots => Entities + ChunkSize;

    public void* Bind<T>(int Include) where T : struct, INativeStruct
    {
        void* Slot = (byte*)Slots + Include * sizeof(NativeRef);
        Unsafe.Write(Slot, Get<T>(Include));
        return Slot;
    }

    public void Dispose()
    {
        if (State != IntPtr.Zero)
        {
            Native.ViewEnd(State);
            State = IntPtr.Zero;
        }
        if (Entities != null)
        {
            NativeMemory.Free(Entities);
            NativeMemory.Free(Pointers);
            Entities = null;
            Pointers = null;
        }
    }
}

public static class ComponentDeconstruct
{
    // Lets foreach ((FEntity Entity, T Component) in View<T>()) keep working over the by-ref enumerator.
    public static void Deconstruct<T>(this T Component, out FEntity Entity, out T Self) where T : struct, IComponent<T>
    {
        Entity = Component.Ref.Entity;
        Self = Component;
    }
}

public readonly struct View<T1> where T1 : struct, IComponent<T1>
{
    private readonly ulong World;
    private readonly Exclude Filter;

    internal View(ulong World, Exclude Filter)
    {
        this.World = World;
        this.Filter = Filter;
    }

    private ViewCursor Open() => new(World, stackalloc IntPtr[] { ComponentOps<T1>.Token }, Filter);

    public void Each(Action<FEntity, T1> Body)
    {
        using ViewCursor Cursor = Open();
        while (Cursor.MoveNext())
        {
            Body(Cursor.Entity, Cursor.Get<T1>(0));
        }
    }

    public Enumerator GetEnumerator() => new(Open());

    public struct Enumerator : IDisposable
    {
        private ViewCursor Cursor;

        internal Enumerator(ViewCursor Cursor) => this.Cursor = Cursor;

        // By ref, so foreach (ref var Component in View) can call setters the IDE sees as mutating.
        public unsafe ref T1 Current => ref Unsafe.AsRef<T1>(Cursor.Bind<T1>(0));

        public bool MoveNext() => Cursor.MoveNext();

        public void Dispose() => Cursor.Dispose();
    }
}

public readonly struct View<T1, T2> where T1 : struct, IComponent<T1> where T2 : struct, IComponent<T2>
{
    private readonly ulong World;
    private readonly Exclude Filter;

    internal View(ulong World, Exclude Filter)
    {
        this.World = World;
        this.Filter = Filter;
    }

    private ViewCursor Open() => new(World, stackalloc IntPtr[] { ComponentOps<T1>.Token, ComponentOps<T2>.Token }, Filter);

    // Each component is a ref into the cursor's slots, so a field set through it lands in storage.
    public readonly unsafe ref struct Row
    {
        private readonly void* Slots;

        internal Row(FEntity Entity, void* Slots)
        {
            this.Entity = Entity;
            this.Slots = Slots;
        }

        public FEntity Entity { get; }
        public ref T1 First => ref Unsafe.AsRef<T1>(Slots);
        public ref T2 Second => ref Unsafe.AsRef<T2>((byte*)Slots + sizeof(NativeRef));

        public void Deconstruct(out FEntity Entity, out T1 First, out T2 Second)
        {
            Entity = this.Entity;
            First = this.First;
            Second = this.Second;
        }
    }

    public void Each(Action<FEntity, T1, T2> Body)
    {
        using ViewCursor Cursor = Open();
        while (Cursor.MoveNext())
        {
            Body(Cursor.Entity, Cursor.Get<T1>(0), Cursor.Get<T2>(1));
        }
    }

    public Enumerator GetEnumerator() => new(Open());

    public struct Enumerator : IDisposable
    {
        private ViewCursor Cursor;

        internal Enumerator(ViewCursor Cursor) => this.Cursor = Cursor;

        public unsafe Row Current
        {
            get
            {
                Cursor.Bind<T1>(0);
                Cursor.Bind<T2>(1);
                return new Row(Cursor.Entity, Cursor.Slots);
            }
        }

        public bool MoveNext() => Cursor.MoveNext();

        public void Dispose() => Cursor.Dispose();
    }
}

public readonly struct View<T1, T2, T3> where T1 : struct, IComponent<T1> where T2 : struct, IComponent<T2> where T3 : struct, IComponent<T3>
{
    private readonly ulong World;
    private readonly Exclude Filter;

    internal View(ulong World, Exclude Filter)
    {
        this.World = World;
        this.Filter = Filter;
    }

    private ViewCursor Open() => new(World, stackalloc IntPtr[] { ComponentOps<T1>.Token, ComponentOps<T2>.Token, ComponentOps<T3>.Token }, Filter);

    // Each component is a ref into the cursor's slots, so a field set through it lands in storage.
    public readonly unsafe ref struct Row
    {
        private readonly void* Slots;

        internal Row(FEntity Entity, void* Slots)
        {
            this.Entity = Entity;
            this.Slots = Slots;
        }

        public FEntity Entity { get; }
        public ref T1 First => ref Unsafe.AsRef<T1>(Slots);
        public ref T2 Second => ref Unsafe.AsRef<T2>((byte*)Slots + sizeof(NativeRef));
        public ref T3 Third => ref Unsafe.AsRef<T3>((byte*)Slots + 2 * sizeof(NativeRef));

        public void Deconstruct(out FEntity Entity, out T1 First, out T2 Second, out T3 Third)
        {
            Entity = this.Entity;
            First = this.First;
            Second = this.Second;
            Third = this.Third;
        }
    }

    public void Each(Action<FEntity, T1, T2, T3> Body)
    {
        using ViewCursor Cursor = Open();
        while (Cursor.MoveNext())
        {
            Body(Cursor.Entity, Cursor.Get<T1>(0), Cursor.Get<T2>(1), Cursor.Get<T3>(2));
        }
    }

    public Enumerator GetEnumerator() => new(Open());

    public struct Enumerator : IDisposable
    {
        private ViewCursor Cursor;

        internal Enumerator(ViewCursor Cursor) => this.Cursor = Cursor;

        public unsafe Row Current
        {
            get
            {
                Cursor.Bind<T1>(0);
                Cursor.Bind<T2>(1);
                Cursor.Bind<T3>(2);
                return new Row(Cursor.Entity, Cursor.Slots);
            }
        }

        public bool MoveNext() => Cursor.MoveNext();

        public void Dispose() => Cursor.Dispose();
    }
}

public readonly struct View<T1, T2, T3, T4> where T1 : struct, IComponent<T1> where T2 : struct, IComponent<T2> where T3 : struct, IComponent<T3> where T4 : struct, IComponent<T4>
{
    private readonly ulong World;
    private readonly Exclude Filter;

    internal View(ulong World, Exclude Filter)
    {
        this.World = World;
        this.Filter = Filter;
    }

    private ViewCursor Open() => new(World, stackalloc IntPtr[] { ComponentOps<T1>.Token, ComponentOps<T2>.Token, ComponentOps<T3>.Token, ComponentOps<T4>.Token }, Filter);

    // Each component is a ref into the cursor's slots, so a field set through it lands in storage.
    public readonly unsafe ref struct Row
    {
        private readonly void* Slots;

        internal Row(FEntity Entity, void* Slots)
        {
            this.Entity = Entity;
            this.Slots = Slots;
        }

        public FEntity Entity { get; }
        public ref T1 First => ref Unsafe.AsRef<T1>(Slots);
        public ref T2 Second => ref Unsafe.AsRef<T2>((byte*)Slots + sizeof(NativeRef));
        public ref T3 Third => ref Unsafe.AsRef<T3>((byte*)Slots + 2 * sizeof(NativeRef));
        public ref T4 Fourth => ref Unsafe.AsRef<T4>((byte*)Slots + 3 * sizeof(NativeRef));

        public void Deconstruct(out FEntity Entity, out T1 First, out T2 Second, out T3 Third, out T4 Fourth)
        {
            Entity = this.Entity;
            First = this.First;
            Second = this.Second;
            Third = this.Third;
            Fourth = this.Fourth;
        }
    }

    public void Each(Action<FEntity, T1, T2, T3, T4> Body)
    {
        using ViewCursor Cursor = Open();
        while (Cursor.MoveNext())
        {
            Body(Cursor.Entity, Cursor.Get<T1>(0), Cursor.Get<T2>(1), Cursor.Get<T3>(2), Cursor.Get<T4>(3));
        }
    }

    public Enumerator GetEnumerator() => new(Open());

    public struct Enumerator : IDisposable
    {
        private ViewCursor Cursor;

        internal Enumerator(ViewCursor Cursor) => this.Cursor = Cursor;

        public unsafe Row Current
        {
            get
            {
                Cursor.Bind<T1>(0);
                Cursor.Bind<T2>(1);
                Cursor.Bind<T3>(2);
                Cursor.Bind<T4>(3);
                return new Row(Cursor.Entity, Cursor.Slots);
            }
        }

        public bool MoveNext() => Cursor.MoveNext();

        public void Dispose() => Cursor.Dispose();
    }
}

public readonly struct View<T1, T2, T3, T4, T5> where T1 : struct, IComponent<T1> where T2 : struct, IComponent<T2> where T3 : struct, IComponent<T3> where T4 : struct, IComponent<T4> where T5 : struct, IComponent<T5>
{
    private readonly ulong World;
    private readonly Exclude Filter;

    internal View(ulong World, Exclude Filter)
    {
        this.World = World;
        this.Filter = Filter;
    }

    private ViewCursor Open() => new(World, stackalloc IntPtr[] { ComponentOps<T1>.Token, ComponentOps<T2>.Token, ComponentOps<T3>.Token, ComponentOps<T4>.Token, ComponentOps<T5>.Token }, Filter);

    // Each component is a ref into the cursor's slots, so a field set through it lands in storage.
    public readonly unsafe ref struct Row
    {
        private readonly void* Slots;

        internal Row(FEntity Entity, void* Slots)
        {
            this.Entity = Entity;
            this.Slots = Slots;
        }

        public FEntity Entity { get; }
        public ref T1 First => ref Unsafe.AsRef<T1>(Slots);
        public ref T2 Second => ref Unsafe.AsRef<T2>((byte*)Slots + sizeof(NativeRef));
        public ref T3 Third => ref Unsafe.AsRef<T3>((byte*)Slots + 2 * sizeof(NativeRef));
        public ref T4 Fourth => ref Unsafe.AsRef<T4>((byte*)Slots + 3 * sizeof(NativeRef));
        public ref T5 Fifth => ref Unsafe.AsRef<T5>((byte*)Slots + 4 * sizeof(NativeRef));

        public void Deconstruct(out FEntity Entity, out T1 First, out T2 Second, out T3 Third, out T4 Fourth, out T5 Fifth)
        {
            Entity = this.Entity;
            First = this.First;
            Second = this.Second;
            Third = this.Third;
            Fourth = this.Fourth;
            Fifth = this.Fifth;
        }
    }

    public void Each(Action<FEntity, T1, T2, T3, T4, T5> Body)
    {
        using ViewCursor Cursor = Open();
        while (Cursor.MoveNext())
        {
            Body(Cursor.Entity, Cursor.Get<T1>(0), Cursor.Get<T2>(1), Cursor.Get<T3>(2), Cursor.Get<T4>(3), Cursor.Get<T5>(4));
        }
    }

    public Enumerator GetEnumerator() => new(Open());

    public struct Enumerator : IDisposable
    {
        private ViewCursor Cursor;

        internal Enumerator(ViewCursor Cursor) => this.Cursor = Cursor;

        public unsafe Row Current
        {
            get
            {
                Cursor.Bind<T1>(0);
                Cursor.Bind<T2>(1);
                Cursor.Bind<T3>(2);
                Cursor.Bind<T4>(3);
                Cursor.Bind<T5>(4);
                return new Row(Cursor.Entity, Cursor.Slots);
            }
        }

        public bool MoveNext() => Cursor.MoveNext();

        public void Dispose() => Cursor.Dispose();
    }
}

public readonly struct View<T1, T2, T3, T4, T5, T6> where T1 : struct, IComponent<T1> where T2 : struct, IComponent<T2> where T3 : struct, IComponent<T3> where T4 : struct, IComponent<T4> where T5 : struct, IComponent<T5> where T6 : struct, IComponent<T6>
{
    private readonly ulong World;
    private readonly Exclude Filter;

    internal View(ulong World, Exclude Filter)
    {
        this.World = World;
        this.Filter = Filter;
    }

    private ViewCursor Open() => new(World, stackalloc IntPtr[] { ComponentOps<T1>.Token, ComponentOps<T2>.Token, ComponentOps<T3>.Token, ComponentOps<T4>.Token, ComponentOps<T5>.Token, ComponentOps<T6>.Token }, Filter);

    // Each component is a ref into the cursor's slots, so a field set through it lands in storage.
    public readonly unsafe ref struct Row
    {
        private readonly void* Slots;

        internal Row(FEntity Entity, void* Slots)
        {
            this.Entity = Entity;
            this.Slots = Slots;
        }

        public FEntity Entity { get; }
        public ref T1 First => ref Unsafe.AsRef<T1>(Slots);
        public ref T2 Second => ref Unsafe.AsRef<T2>((byte*)Slots + sizeof(NativeRef));
        public ref T3 Third => ref Unsafe.AsRef<T3>((byte*)Slots + 2 * sizeof(NativeRef));
        public ref T4 Fourth => ref Unsafe.AsRef<T4>((byte*)Slots + 3 * sizeof(NativeRef));
        public ref T5 Fifth => ref Unsafe.AsRef<T5>((byte*)Slots + 4 * sizeof(NativeRef));
        public ref T6 Sixth => ref Unsafe.AsRef<T6>((byte*)Slots + 5 * sizeof(NativeRef));

        public void Deconstruct(out FEntity Entity, out T1 First, out T2 Second, out T3 Third, out T4 Fourth, out T5 Fifth, out T6 Sixth)
        {
            Entity = this.Entity;
            First = this.First;
            Second = this.Second;
            Third = this.Third;
            Fourth = this.Fourth;
            Fifth = this.Fifth;
            Sixth = this.Sixth;
        }
    }

    public void Each(Action<FEntity, T1, T2, T3, T4, T5, T6> Body)
    {
        using ViewCursor Cursor = Open();
        while (Cursor.MoveNext())
        {
            Body(Cursor.Entity, Cursor.Get<T1>(0), Cursor.Get<T2>(1), Cursor.Get<T3>(2), Cursor.Get<T4>(3), Cursor.Get<T5>(4), Cursor.Get<T6>(5));
        }
    }

    public Enumerator GetEnumerator() => new(Open());

    public struct Enumerator : IDisposable
    {
        private ViewCursor Cursor;

        internal Enumerator(ViewCursor Cursor) => this.Cursor = Cursor;

        public unsafe Row Current
        {
            get
            {
                Cursor.Bind<T1>(0);
                Cursor.Bind<T2>(1);
                Cursor.Bind<T3>(2);
                Cursor.Bind<T4>(3);
                Cursor.Bind<T5>(4);
                Cursor.Bind<T6>(5);
                return new Row(Cursor.Entity, Cursor.Slots);
            }
        }

        public bool MoveNext() => Cursor.MoveNext();

        public void Dispose() => Cursor.Dispose();
    }
}

public readonly struct View<T1, T2, T3, T4, T5, T6, T7> where T1 : struct, IComponent<T1> where T2 : struct, IComponent<T2> where T3 : struct, IComponent<T3> where T4 : struct, IComponent<T4> where T5 : struct, IComponent<T5> where T6 : struct, IComponent<T6> where T7 : struct, IComponent<T7>
{
    private readonly ulong World;
    private readonly Exclude Filter;

    internal View(ulong World, Exclude Filter)
    {
        this.World = World;
        this.Filter = Filter;
    }

    private ViewCursor Open() => new(World, stackalloc IntPtr[] { ComponentOps<T1>.Token, ComponentOps<T2>.Token, ComponentOps<T3>.Token, ComponentOps<T4>.Token, ComponentOps<T5>.Token, ComponentOps<T6>.Token, ComponentOps<T7>.Token }, Filter);

    // Each component is a ref into the cursor's slots, so a field set through it lands in storage.
    public readonly unsafe ref struct Row
    {
        private readonly void* Slots;

        internal Row(FEntity Entity, void* Slots)
        {
            this.Entity = Entity;
            this.Slots = Slots;
        }

        public FEntity Entity { get; }
        public ref T1 First => ref Unsafe.AsRef<T1>(Slots);
        public ref T2 Second => ref Unsafe.AsRef<T2>((byte*)Slots + sizeof(NativeRef));
        public ref T3 Third => ref Unsafe.AsRef<T3>((byte*)Slots + 2 * sizeof(NativeRef));
        public ref T4 Fourth => ref Unsafe.AsRef<T4>((byte*)Slots + 3 * sizeof(NativeRef));
        public ref T5 Fifth => ref Unsafe.AsRef<T5>((byte*)Slots + 4 * sizeof(NativeRef));
        public ref T6 Sixth => ref Unsafe.AsRef<T6>((byte*)Slots + 5 * sizeof(NativeRef));
        public ref T7 Seventh => ref Unsafe.AsRef<T7>((byte*)Slots + 6 * sizeof(NativeRef));

        public void Deconstruct(out FEntity Entity, out T1 First, out T2 Second, out T3 Third, out T4 Fourth, out T5 Fifth, out T6 Sixth, out T7 Seventh)
        {
            Entity = this.Entity;
            First = this.First;
            Second = this.Second;
            Third = this.Third;
            Fourth = this.Fourth;
            Fifth = this.Fifth;
            Sixth = this.Sixth;
            Seventh = this.Seventh;
        }
    }

    public void Each(Action<FEntity, T1, T2, T3, T4, T5, T6, T7> Body)
    {
        using ViewCursor Cursor = Open();
        while (Cursor.MoveNext())
        {
            Body(Cursor.Entity, Cursor.Get<T1>(0), Cursor.Get<T2>(1), Cursor.Get<T3>(2), Cursor.Get<T4>(3), Cursor.Get<T5>(4), Cursor.Get<T6>(5), Cursor.Get<T7>(6));
        }
    }

    public Enumerator GetEnumerator() => new(Open());

    public struct Enumerator : IDisposable
    {
        private ViewCursor Cursor;

        internal Enumerator(ViewCursor Cursor) => this.Cursor = Cursor;

        public unsafe Row Current
        {
            get
            {
                Cursor.Bind<T1>(0);
                Cursor.Bind<T2>(1);
                Cursor.Bind<T3>(2);
                Cursor.Bind<T4>(3);
                Cursor.Bind<T5>(4);
                Cursor.Bind<T6>(5);
                Cursor.Bind<T7>(6);
                return new Row(Cursor.Entity, Cursor.Slots);
            }
        }

        public bool MoveNext() => Cursor.MoveNext();

        public void Dispose() => Cursor.Dispose();
    }
}

public readonly struct View<T1, T2, T3, T4, T5, T6, T7, T8> where T1 : struct, IComponent<T1> where T2 : struct, IComponent<T2> where T3 : struct, IComponent<T3> where T4 : struct, IComponent<T4> where T5 : struct, IComponent<T5> where T6 : struct, IComponent<T6> where T7 : struct, IComponent<T7> where T8 : struct, IComponent<T8>
{
    private readonly ulong World;
    private readonly Exclude Filter;

    internal View(ulong World, Exclude Filter)
    {
        this.World = World;
        this.Filter = Filter;
    }

    private ViewCursor Open() => new(World, stackalloc IntPtr[] { ComponentOps<T1>.Token, ComponentOps<T2>.Token, ComponentOps<T3>.Token, ComponentOps<T4>.Token, ComponentOps<T5>.Token, ComponentOps<T6>.Token, ComponentOps<T7>.Token, ComponentOps<T8>.Token }, Filter);

    // Each component is a ref into the cursor's slots, so a field set through it lands in storage.
    public readonly unsafe ref struct Row
    {
        private readonly void* Slots;

        internal Row(FEntity Entity, void* Slots)
        {
            this.Entity = Entity;
            this.Slots = Slots;
        }

        public FEntity Entity { get; }
        public ref T1 First => ref Unsafe.AsRef<T1>(Slots);
        public ref T2 Second => ref Unsafe.AsRef<T2>((byte*)Slots + sizeof(NativeRef));
        public ref T3 Third => ref Unsafe.AsRef<T3>((byte*)Slots + 2 * sizeof(NativeRef));
        public ref T4 Fourth => ref Unsafe.AsRef<T4>((byte*)Slots + 3 * sizeof(NativeRef));
        public ref T5 Fifth => ref Unsafe.AsRef<T5>((byte*)Slots + 4 * sizeof(NativeRef));
        public ref T6 Sixth => ref Unsafe.AsRef<T6>((byte*)Slots + 5 * sizeof(NativeRef));
        public ref T7 Seventh => ref Unsafe.AsRef<T7>((byte*)Slots + 6 * sizeof(NativeRef));
        public ref T8 Eighth => ref Unsafe.AsRef<T8>((byte*)Slots + 7 * sizeof(NativeRef));

        public void Deconstruct(out FEntity Entity, out T1 First, out T2 Second, out T3 Third, out T4 Fourth, out T5 Fifth, out T6 Sixth, out T7 Seventh, out T8 Eighth)
        {
            Entity = this.Entity;
            First = this.First;
            Second = this.Second;
            Third = this.Third;
            Fourth = this.Fourth;
            Fifth = this.Fifth;
            Sixth = this.Sixth;
            Seventh = this.Seventh;
            Eighth = this.Eighth;
        }
    }

    public void Each(Action<FEntity, T1, T2, T3, T4, T5, T6, T7, T8> Body)
    {
        using ViewCursor Cursor = Open();
        while (Cursor.MoveNext())
        {
            Body(Cursor.Entity, Cursor.Get<T1>(0), Cursor.Get<T2>(1), Cursor.Get<T3>(2), Cursor.Get<T4>(3), Cursor.Get<T5>(4), Cursor.Get<T6>(5), Cursor.Get<T7>(6), Cursor.Get<T8>(7));
        }
    }

    public Enumerator GetEnumerator() => new(Open());

    public struct Enumerator : IDisposable
    {
        private ViewCursor Cursor;

        internal Enumerator(ViewCursor Cursor) => this.Cursor = Cursor;

        public unsafe Row Current
        {
            get
            {
                Cursor.Bind<T1>(0);
                Cursor.Bind<T2>(1);
                Cursor.Bind<T3>(2);
                Cursor.Bind<T4>(3);
                Cursor.Bind<T5>(4);
                Cursor.Bind<T6>(5);
                Cursor.Bind<T7>(6);
                Cursor.Bind<T8>(7);
                return new Row(Cursor.Entity, Cursor.Slots);
            }
        }

        public bool MoveNext() => Cursor.MoveNext();

        public void Dispose() => Cursor.Dispose();
    }
}
