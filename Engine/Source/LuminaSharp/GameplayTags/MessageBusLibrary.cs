using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using LuminaSharp;

namespace Lumina;

// Typed sending and listening over the world's native message bus, so C++ and C# send to and hear each other.
public unsafe partial class CMessageBusLibrary
{
    public static IDisposable Subscribe<T>(CWorld World, GameplayTag Channel, Action<T> Handler, GameplayTagMatch Match = GameplayTagMatch.Partial)
        => Listen<T>(World, FEntity.Null, Channel, Message => { Handler(Message); return false; }, Match);

    public static IDisposable Subscribe<T>(CWorld World, string Channel, Action<T> Handler, GameplayTagMatch Match = GameplayTagMatch.Partial)
        => Subscribe(World, GameplayTag.Request(Channel), Handler, Match);

    public static IDisposable SubscribeOnce<T>(CWorld World, GameplayTag Channel, Action<T> Handler, GameplayTagMatch Match = GameplayTagMatch.Partial)
    {
        IDisposable? Subscription = null;
        Subscription = Subscribe<T>(World, Channel, Message => { Subscription?.Dispose(); Handler(Message); }, Match);
        return Subscription;
    }

    public static IDisposable SubscribeOnce<T>(CWorld World, string Channel, Action<T> Handler, GameplayTagMatch Match = GameplayTagMatch.Partial)
        => SubscribeOnce(World, GameplayTag.Request(Channel), Handler, Match);

    // Hears routed sends that reach Owner, and the bus drops it when the entity dies.
    public static IDisposable Subscribe<T>(CWorld World, FEntity Owner, GameplayTag Channel, Action<T> Handler, GameplayTagMatch Match = GameplayTagMatch.Partial)
        => Owner.IsNull ? GameplayMessageSubscription.Empty : Listen<T>(World, Owner, Channel, Message => { Handler(Message); return false; }, Match);

    public static IDisposable Subscribe<T>(CWorld World, FEntity Owner, string Channel, Action<T> Handler, GameplayTagMatch Match = GameplayTagMatch.Partial)
        => Subscribe(World, Owner, GameplayTag.Request(Channel), Handler, Match);

    // Returning true marks the message handled, so no further ancestor or descendant receives it.
    public static IDisposable Subscribe<T>(CWorld World, FEntity Owner, GameplayTag Channel, Func<T, bool> Handler, GameplayTagMatch Match = GameplayTagMatch.Partial)
        => Owner.IsNull ? GameplayMessageSubscription.Empty : Listen(World, Owner, Channel, Handler, Match);

    public static IDisposable Subscribe<T>(CWorld World, FEntity Owner, string Channel, Func<T, bool> Handler, GameplayTagMatch Match = GameplayTagMatch.Partial)
        => Subscribe(World, Owner, GameplayTag.Request(Channel), Handler, Match);

    // Partial listeners on the channel's ancestors hear it too.
    public static void Broadcast<T>(CWorld World, GameplayTag Channel, T Message) => Send(World, MessageRoute.Broadcast, FEntity.Null, Channel, Message, true);

    public static void Broadcast<T>(CWorld World, string Channel, T Message) => Broadcast(World, GameplayTag.Request(Channel), Message);

    public static void SendUp<T>(CWorld World, FEntity Source, GameplayTag Channel, T Message, bool IncludeSelf = true)
        => Send(World, MessageRoute.Up, Source, Channel, Message, IncludeSelf);

    public static void SendUp<T>(CWorld World, FEntity Source, string Channel, T Message, bool IncludeSelf = true)
        => SendUp(World, Source, GameplayTag.Request(Channel), Message, IncludeSelf);

    public static void SendDown<T>(CWorld World, FEntity Source, GameplayTag Channel, T Message, bool IncludeSelf = true)
        => Send(World, MessageRoute.Down, Source, Channel, Message, IncludeSelf);

    public static void SendDown<T>(CWorld World, FEntity Source, string Channel, T Message, bool IncludeSelf = true)
        => SendDown(World, Source, GameplayTag.Request(Channel), Message, IncludeSelf);

    public static void SendTo<T>(CWorld World, FEntity Target, GameplayTag Channel, T Message) => Send(World, MessageRoute.To, Target, Channel, Message, true);

    public static void SendTo<T>(CWorld World, FEntity Target, string Channel, T Message) => SendTo(World, Target, GameplayTag.Request(Channel), Message);

    private static bool IsLive(CWorld? World) => World is { IsValid: true };

    private static IDisposable Listen<T>(CWorld World, FEntity Owner, GameplayTag Channel, Func<T, bool> Handler, GameplayTagMatch Match)
    {
        if (!Channel.IsValid || !IsLive(World))
        {
            return GameplayMessageSubscription.Empty;
        }

        // Handed to the native bus, which releases it when the listener goes.
        ulong Id = Subscribe(World, Channel.Id, Owner, MessageType<T>.Key, Match == GameplayTagMatch.Exact,
                             ScriptCallback.Bind(new MessageInvoker<T>(Handler)));
        return Id == 0 ? GameplayMessageSubscription.Empty : new GameplayMessageSubscription(World, Id);
    }

    private static void Send<T>(CWorld World, MessageRoute Route, FEntity Target, GameplayTag Channel, T Message, bool IncludeSelf)
    {
        if (!Channel.IsValid || (Route != MessageRoute.Broadcast && Target.IsNull) || Message is null || !IsLive(World))
        {
            return;
        }

        switch (MessageType<T>.Kind)
        {
            case PayloadKind.Bytes:
            {
                T Copy = Message;
                Send(World, (uint)Route, Target, Channel.Id, (uint)PayloadKind.Bytes, MessageType<T>.Key,
                     (IntPtr)Unsafe.AsPointer(ref Copy), (uint)Unsafe.SizeOf<T>(), IntPtr.Zero, IncludeSelf);
                break;
            }
            case PayloadKind.Struct:
            {
                T Copy = Message;
                Send(World, (uint)Route, Target, Channel.Id, (uint)PayloadKind.Struct, MessageType<T>.Key,
                     Unsafe.As<T, NativeRef>(ref Copy).Address, 0, MessageType<T>.Struct, IncludeSelf);
                break;
            }
            default:
            {
                // The native bus frees the handle once the message is delivered.
                GCHandle Payload = GCHandle.Alloc(Message);
                Send(World, (uint)Route, Target, Channel.Id, (uint)PayloadKind.Managed, MessageType.ManagedKey,
                     GCHandle.ToIntPtr(Payload), 0, IntPtr.Zero, IncludeSelf);
                break;
            }
        }
    }
}
