module dbey.hex;

private enum char[16] HEX_DIGITS = "0123456789ABCDEF";

private T toHexDigit(T)(ubyte nibble)
{
  return cast(T) HEX_DIGITS[nibble];
}

auto toHexRange(T)(const(char)[] str)
{
  static assert(is(typeof(cast(T) '0')), "T must be constructible from a char value.");

  static struct HexRange
  {
    const(char)[] source;
    size_t index;
    bool highNibble = true;

    @property bool empty() const
    {
      return index >= source.length;
    }

    @property T front() const
    {
      const b = cast(ubyte) source[index];
      const nibble = cast(ubyte) (highNibble ? (b >> 4) : (b & 0x0F));
      return toHexDigit!T(nibble);
    }

    void popFront()
    {
      if (highNibble)
      {
        highNibble = false;
      }
      else
      {
        highNibble = true;
        ++index;
      }
    }
  }

  return HexRange(str, 0, true);
}

auto toHex(T)(const(char)[] str)
{
  static assert(is(typeof(cast(T) '0')), "T must be constructible from a char value.");

  static if (is(T : string))
  {
    auto result = new char[str.length * 2];
    size_t out;

    foreach (c; str)
    {
      const b = cast(ubyte) c;
      result[out++] = HEX_DIGITS[b >> 4];
      result[out++] = HEX_DIGITS[b & 0x0F];
    }

    return cast(string) result;
  }
  else
  {
    auto result = new T[str.length * 2];
    size_t out;

    foreach (c; str)
    {
      const b = cast(ubyte) c;
      result[out++] = toHexDigit!T(cast(ubyte) (b >> 4));
      result[out++] = toHexDigit!T(cast(ubyte) (b & 0x0F));
    }

    return result;
  }
}

unittest
{
  enum expected = "61626320C3A4C3B6C3BC50C39FE282AC207B447D";
  enum str = "abc äöüPß€ {D}";

  assert(str.toHex!string == expected);

  import std.conv : hexString;

  static assert(hexString!expected == str);

  enum NATO
  {
    Alpha = '0', Bravo, Charlie, Delta, Echo,
    Foxtrot, Golf, Hotel, India, Juliett,
    Kilo = 'A', Lima, Mike, November, Oscar, Papa
  }

  auto test = str.toHex!NATO;
  assert(is(typeof(test) : NATO[]));

  foreach (i, c; expected)
  {
    assert(cast(char) test[i] == c);
  }
}
