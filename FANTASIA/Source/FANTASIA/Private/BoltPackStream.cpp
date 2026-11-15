/**
 * @file BoltPackStream.cpp
 * @brief Implementation of Bolt PackStream reader, writer, and value-to-string conversion.
 */

#include "BoltPackStream.h"

FString FBoltValue::AsString() const
{
	switch (Type)
	{
	case EBoltValueType::Null: return TEXT("");
	case EBoltValueType::Bool: return BoolVal ? TEXT("true") : TEXT("false");
	case EBoltValueType::Int: return FString::Printf(TEXT("%lld"), IntVal);
	case EBoltValueType::Float: return FString::SanitizeFloat(FloatVal);
	case EBoltValueType::String: return StringVal;
	case EBoltValueType::List:
	{
		FString Result = TEXT("[");
		for (int32 i = 0; i < ListVal.Num(); i++)
		{
			if (i > 0) Result += TEXT(", ");
			Result += ListVal[i]->AsString();
		}
		return Result + TEXT("]");
	}
	case EBoltValueType::Map:
	{
		FString Result = TEXT("{");
		bool First = true;
		for (const auto& Pair : MapVal)
		{
			if (!First) Result += TEXT(", ");
			Result += Pair.Key + TEXT(": ") + Pair.Value->AsString();
			First = false;
		}
		return Result + TEXT("}");
	}
	case EBoltValueType::Structure:
		return FString::Printf(TEXT("Structure(0x%02X)"), StructVal.Tag);
	default: return TEXT("");
	}
}

// ============================================================================
// Writer — inlined single-byte writes, pre-allocated buffer
// ============================================================================

FORCEINLINE void FBoltPackStreamWriter::WriteByte(uint8 B) { Buffer.Add(B); }

FORCEINLINE void FBoltPackStreamWriter::WriteUInt16BE(uint16 V)
{
	uint8 Tmp[2] = { uint8(V >> 8), uint8(V & 0xFF) };
	Buffer.Append(Tmp, 2);
}

FORCEINLINE void FBoltPackStreamWriter::WriteInt16BE(int16 V) { WriteUInt16BE(static_cast<uint16>(V)); }

FORCEINLINE void FBoltPackStreamWriter::WriteInt32BE(int32 V)
{
	uint8 Tmp[4] = { uint8((V>>24)&0xFF), uint8((V>>16)&0xFF), uint8((V>>8)&0xFF), uint8(V&0xFF) };
	Buffer.Append(Tmp, 4);
}

FORCEINLINE void FBoltPackStreamWriter::WriteInt64BE(int64 V)
{
	uint8 Tmp[8];
	for (int i = 0; i < 8; i++) Tmp[i] = static_cast<uint8>((V >> (56 - i*8)) & 0xFF);
	Buffer.Append(Tmp, 8);
}

FORCEINLINE void FBoltPackStreamWriter::WriteFloat64BE(double V)
{
	uint64 B;
	FMemory::Memcpy(&B, &V, 8);
	uint8 Tmp[8];
	for (int i = 0; i < 8; i++) Tmp[i] = static_cast<uint8>((B >> (56 - i*8)) & 0xFF);
	Buffer.Append(Tmp, 8);
}

void FBoltPackStreamWriter::WriteNull() { WriteByte(0xC0); }
void FBoltPackStreamWriter::WriteBool(bool V) { WriteByte(V ? 0xC3 : 0xC2); }

void FBoltPackStreamWriter::WriteInt(int64 V)
{
	if (V >= -16 && V <= 127) { WriteByte(static_cast<uint8>(static_cast<int8>(V))); }
	else if (V >= -128 && V <= 127) { WriteByte(0xC8); WriteByte(static_cast<uint8>(static_cast<int8>(V))); }
	else if (V >= -32768 && V <= 32767) { WriteByte(0xC9); WriteInt16BE(static_cast<int16>(V)); }
	else if (V >= INT32_MIN && V <= INT32_MAX) { WriteByte(0xCA); WriteInt32BE(static_cast<int32>(V)); }
	else { WriteByte(0xCB); WriteInt64BE(V); }
}

void FBoltPackStreamWriter::WriteFloat(double V) { WriteByte(0xC1); WriteFloat64BE(V); }

void FBoltPackStreamWriter::WriteString(const FString& Value)
{
	FTCHARToUTF8 Conv(*Value);
	const int32 Len = Conv.Length();
	if (Len <= 15) WriteByte(0x80 | static_cast<uint8>(Len));
	else if (Len <= 255) { WriteByte(0xD0); WriteByte(static_cast<uint8>(Len)); }
	else if (Len <= 65535) { WriteByte(0xD1); WriteUInt16BE(static_cast<uint16>(Len)); }
	else { WriteByte(0xD2); WriteInt32BE(Len); }
	Buffer.Append(reinterpret_cast<const uint8*>(Conv.Get()), Len);
}

void FBoltPackStreamWriter::WriteBytes(const TArray<uint8>& Value)
{
	const int32 Len = Value.Num();
	if (Len <= 255) { WriteByte(0xCC); WriteByte(static_cast<uint8>(Len)); }
	else if (Len <= 65535) { WriteByte(0xCD); WriteUInt16BE(static_cast<uint16>(Len)); }
	else { WriteByte(0xCE); WriteInt32BE(Len); }
	Buffer.Append(Value.GetData(), Len);
}

void FBoltPackStreamWriter::WriteListHeader(int32 S)
{
	if (S <= 15) WriteByte(0x90 | static_cast<uint8>(S));
	else if (S <= 255) { WriteByte(0xD4); WriteByte(static_cast<uint8>(S)); }
	else if (S <= 65535) { WriteByte(0xD5); WriteUInt16BE(static_cast<uint16>(S)); }
	else { WriteByte(0xD6); WriteInt32BE(S); }
}

void FBoltPackStreamWriter::WriteMapHeader(int32 S)
{
	if (S <= 15) WriteByte(0xA0 | static_cast<uint8>(S));
	else if (S <= 255) { WriteByte(0xD8); WriteByte(static_cast<uint8>(S)); }
	else if (S <= 65535) { WriteByte(0xD9); WriteUInt16BE(static_cast<uint16>(S)); }
	else { WriteByte(0xDA); WriteInt32BE(S); }
}

void FBoltPackStreamWriter::WriteStructureHeader(uint8 Tag, int32 N)
{
	uint8 Tmp[2] = { uint8(0xB0 | (N & 0x0F)), Tag };
	Buffer.Append(Tmp, 2);
}

void FBoltPackStreamWriter::WriteValue(const FBoltValuePtr& Value)
{
	if (!Value) { WriteNull(); return; }
	switch (Value->Type)
	{
	case EBoltValueType::Null: WriteNull(); break;
	case EBoltValueType::Bool: WriteBool(Value->BoolVal); break;
	case EBoltValueType::Int: WriteInt(Value->IntVal); break;
	case EBoltValueType::Float: WriteFloat(Value->FloatVal); break;
	case EBoltValueType::String: WriteString(Value->StringVal); break;
	case EBoltValueType::Bytes: WriteBytes(Value->BytesVal); break;
	case EBoltValueType::List:
		WriteListHeader(Value->ListVal.Num());
		for (const auto& Item : Value->ListVal) WriteValue(Item);
		break;
	case EBoltValueType::Map:
		WriteMapHeader(Value->MapVal.Num());
		for (const auto& Pair : Value->MapVal) { WriteString(Pair.Key); WriteValue(Pair.Value); }
		break;
	case EBoltValueType::Structure:
		WriteStructureHeader(Value->StructVal.Tag, Value->StructVal.Fields.Num());
		for (const auto& Field : Value->StructVal.Fields) WriteValue(Field);
		break;
	}
}

// ============================================================================
// Reader — every read bounded against the buffer before it happens
// ============================================================================

/**
 * Deepest nesting of lists, maps and structures the reader will decode.
 *
 * ReadValue recurses once per enclosing container, so without a limit a message
 * of N repeated 0x91 bytes is N stack frames. Nothing a Neo4j server builds from
 * stored data comes close: the deepest shape the Bolt layer meets is a RECORD's
 * field list holding a Path structure, whose node list holds a Node structure,
 * whose property map holds a list — eight levels. 64 leaves eight times that
 * headroom for a Cypher query returning its own nested literals, while bounding
 * the reader's stack use to a few kilobytes.
 */
static constexpr int32 MaxBoltNestingDepth = 64;

FBoltPackStreamReader::FBoltPackStreamReader(const TArray<uint8>& InData) : Data(InData) {}
bool FBoltPackStreamReader::HasMore() const { return !bRefused && Cursor < Data.Num(); }
uint8 FBoltPackStreamReader::PeekMarker() const { return (Cursor < Data.Num()) ? Data[Cursor] : 0; }

void FBoltPackStreamReader::Refuse(const FString& Reason)
{
	// Only the first refusal reaches here: Require() short-circuits on bRefused.
	bRefused = true;
	Cursor = Data.Num();
	UE_LOG(LogTemp, Error, TEXT("BoltPackStream: refused a %d-byte message — %s"), Data.Num(), *Reason);
}

FORCEINLINE bool FBoltPackStreamReader::Require(int32 NumBytes)
{
	if (bRefused) return false;
	if (NumBytes < 0 || NumBytes > Data.Num() - Cursor)
	{
		Refuse(FString::Printf(TEXT("needs %d more bytes at offset %d, %d remain"),
			NumBytes, Cursor, Data.Num() - Cursor));
		return false;
	}
	return true;
}

FORCEINLINE uint8 FBoltPackStreamReader::ReadByte()
{
	if (!Require(1)) return 0;
	return Data.GetData()[Cursor++];
}

FORCEINLINE uint16 FBoltPackStreamReader::ReadUInt16BE()
{
	if (!Require(2)) return 0;
	const uint8* P = Data.GetData() + Cursor;
	Cursor += 2;
	return (uint16(P[0]) << 8) | P[1];
}

FORCEINLINE int16 FBoltPackStreamReader::ReadInt16BE() { return static_cast<int16>(ReadUInt16BE()); }

FORCEINLINE int32 FBoltPackStreamReader::ReadInt32BE()
{
	if (!Require(4)) return 0;
	const uint8* P = Data.GetData() + Cursor;
	Cursor += 4;
	return (int32(P[0]) << 24) | (int32(P[1]) << 16) | (int32(P[2]) << 8) | P[3];
}

FORCEINLINE int64 FBoltPackStreamReader::ReadInt64BE()
{
	if (!Require(8)) return 0;
	const uint8* P = Data.GetData() + Cursor;
	Cursor += 8;
	int64 R = 0;
	for (int i = 0; i < 8; i++) R = (R << 8) | P[i];
	return R;
}

FORCEINLINE double FBoltPackStreamReader::ReadFloat64BE()
{
	const uint64 B = static_cast<uint64>(ReadInt64BE());
	double R;
	FMemory::Memcpy(&R, &B, 8);
	return R;
}

FString FBoltPackStreamReader::ReadStringOfLength(int32 Len)
{
	if (!Require(Len) || Len == 0) return TEXT("");
	FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Data.GetData() + Cursor), Len);
	Cursor += Len;
	return FString(Conv.Length(), Conv.Get());
}

FBoltValuePtr FBoltPackStreamReader::ReadList(int32 Count, int32 Depth)
{
	// Reject before reserving: a count larger than the bytes left cannot be met.
	if (!Require(Count)) return FBoltValue::MakeNull();
	FBoltValueArray Items;
	Items.Reserve(Count);
	for (int32 i = 0; i < Count && !bRefused; i++) Items.Add(ReadValue(Depth + 1));
	return FBoltValue::MakeList(MoveTemp(Items));
}

FBoltValuePtr FBoltPackStreamReader::ReadMap(int32 Count, int32 Depth)
{
	if (!Require(Count)) return FBoltValue::MakeNull();
	FBoltValueMap Map;
	Map.Reserve(Count);
	for (int32 i = 0; i < Count && !bRefused; i++)
	{
		auto K = ReadValue(Depth + 1);
		Map.Add(K->StringVal, ReadValue(Depth + 1));
	}
	return FBoltValue::MakeMap(MoveTemp(Map));
}

FBoltValuePtr FBoltPackStreamReader::ReadStructure(int32 NumFields, int32 Depth)
{
	// One tag byte plus at least one byte per field.
	if (!Require(NumFields + 1)) return FBoltValue::MakeNull();
	const uint8 Tag = ReadByte();
	FBoltValueArray Fields;
	Fields.Reserve(NumFields);
	for (int32 i = 0; i < NumFields && !bRefused; i++) Fields.Add(ReadValue(Depth + 1));
	return FBoltValue::MakeStructure(Tag, MoveTemp(Fields));
}

FBoltValuePtr FBoltPackStreamReader::ReadValue() { return ReadValue(0); }

FBoltValuePtr FBoltPackStreamReader::ReadValue(int32 Depth)
{
	if (bRefused) return FBoltValue::MakeNull();

	if (Depth >= MaxBoltNestingDepth)
	{
		Refuse(FString::Printf(TEXT("nested past %d containers at offset %d"), MaxBoltNestingDepth, Cursor));
		return FBoltValue::MakeNull();
	}

	// One funnel for every refusal: a value decoded over refused bytes is never
	// returned, at any level of the recursion.
	FBoltValuePtr Value = ReadMarkedValue(Depth);
	return bRefused ? FBoltValue::MakeNull() : Value;
}

FBoltValuePtr FBoltPackStreamReader::ReadMarkedValue(int32 Depth)
{
	const uint8 M = ReadByte();
	if (bRefused) return FBoltValue::MakeNull();

	// Tiny int positive: 0x00-0x7F
	if (M <= 0x7F) return FBoltValue::MakeInt(static_cast<int64>(M));
	// Tiny int negative: 0xF0-0xFF
	if (M >= 0xF0) return FBoltValue::MakeInt(static_cast<int64>(static_cast<int8>(M)));

	// Tiny string: 0x80-0x8F
	if (M >= 0x80 && M <= 0x8F) return FBoltValue::MakeString(ReadStringOfLength(M & 0x0F));
	// Tiny list: 0x90-0x9F
	if (M >= 0x90 && M <= 0x9F) return ReadList(M & 0x0F, Depth);
	// Tiny map: 0xA0-0xAF
	if (M >= 0xA0 && M <= 0xAF) return ReadMap(M & 0x0F, Depth);
	// Tiny structure: 0xB0-0xBF
	if (M >= 0xB0 && M <= 0xBF) return ReadStructure(M & 0x0F, Depth);

	switch (M)
	{
	case 0xC0: return FBoltValue::MakeNull();
	case 0xC1: return FBoltValue::MakeFloat(ReadFloat64BE());
	case 0xC2: return FBoltValue::MakeBool(false);
	case 0xC3: return FBoltValue::MakeBool(true);
	case 0xC8: return FBoltValue::MakeInt(static_cast<int64>(static_cast<int8>(ReadByte())));
	case 0xC9: return FBoltValue::MakeInt(static_cast<int64>(ReadInt16BE()));
	case 0xCA: return FBoltValue::MakeInt(static_cast<int64>(ReadInt32BE()));
	case 0xCB: return FBoltValue::MakeInt(ReadInt64BE());

	// Bytes8/16/32
	case 0xCC: case 0xCD: case 0xCE:
	{
		const int32 Len = (M == 0xCC) ? ReadByte() : (M == 0xCD) ? ReadUInt16BE() : ReadInt32BE();
		if (!Require(Len)) return FBoltValue::MakeNull();
		auto V = MakeShared<FBoltValue>();
		V->Type = EBoltValueType::Bytes;
		V->BytesVal.Append(Data.GetData() + Cursor, Len);
		Cursor += Len;
		return V;
	}

	// String8/16/32
	case 0xD0: case 0xD1: case 0xD2:
	{
		const int32 Len = (M == 0xD0) ? ReadByte() : (M == 0xD1) ? ReadUInt16BE() : ReadInt32BE();
		return FBoltValue::MakeString(ReadStringOfLength(Len));
	}

	// List8/16/32
	case 0xD4: case 0xD5: case 0xD6:
		return ReadList((M == 0xD4) ? ReadByte() : (M == 0xD5) ? ReadUInt16BE() : ReadInt32BE(), Depth);

	// Map8/16/32
	case 0xD8: case 0xD9: case 0xDA:
		return ReadMap((M == 0xD8) ? ReadByte() : (M == 0xD9) ? ReadUInt16BE() : ReadInt32BE(), Depth);

	default:
		UE_LOG(LogTemp, Error, TEXT("BoltPackStream: Unknown marker 0x%02X at offset %d"), M, Cursor - 1);
		return FBoltValue::MakeNull();
	}
}
