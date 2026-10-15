using System;
using LuminaSharp;

namespace Lumina;

// Typed access to an animation graph's parameter block, which the graph asset names as a reflected struct.
public readonly unsafe partial struct SAnimationGraphComponent
{
    // Goes through the native accessor rather than Parameters, since it initializes the block from the graph first.
    public T GetParameters<T>() where T : struct, INativeStruct<T>
    {
        if (!TryGetParameters(out T Value))
        {
            throw new InvalidOperationException($"Animation graph parameter block is not a {T.NativeName}.");
        }
        return Value;
    }

    // False with an empty handle when the graph's parameter block is not a T.
    public bool TryGetParameters<T>(out T Value) where T : struct, INativeStruct<T>
    {
        nint Memory = GetParameterMemoryAs(T.NativeName);
        Value = NativeStructs.FromAddress<T>(Memory);
        return Memory != 0;
    }
}
