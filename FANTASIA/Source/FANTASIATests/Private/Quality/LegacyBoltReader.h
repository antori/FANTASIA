/**
 * @file LegacyBoltReader.h
 * @brief The PackStream reader as it stood before US-030, kept as a differential oracle.
 *
 * A copy of `FBoltPackStreamReader::ReadValue` from the commit that closed S-01, with no bounds
 * checks at all. It is correct for well-formed input and undefined for anything else, which is
 * exactly why it is only ever fed buffers the writer produced. Two uses:
 *
 *   - `Quality.Bolt.Differential.*` asserts the bounded reader decodes every well-formed value to
 *     the same tree this one does, so the fix refused nothing it should have decoded.
 *   - `Stress.Bolt.Reader.BoundsCheckCost` times both over the same bytes, which is the only way to
 *     say what the checks cost: S-01 left no baseline and the old reader is gone from the source.
 *
 * Test code under Quality/, not source. It reads through a raw pointer and trusts every length.
 */

#pragma once

#include "CoreMinimal.h"
#include "BoltPackStream.h"

namespace FantasiaLegacyBolt
{
class FReader
{
public:
	explicit FReader(const TArray<uint8>& InData) : Data(InData) {}

	FBoltValuePtr ReadValue()
	{
		const uint8 M = ReadByte();

		if (M <= 0x7F) return FBoltValue::MakeInt(static_cast<int64>(M));
		if (M >= 0xF0) return FBoltValue::MakeInt(static_cast<int64>(static_cast<int8>(M)));
		if (M >= 0x80 && M <= 0x8F) return FBoltValue::MakeString(ReadStringOfLength(M & 0x0F));

		if (M >= 0x90 && M <= 0x9F) return ReadList(M & 0x0F);
		if (M >= 0xA0 && M <= 0xAF) return ReadMap(M & 0x0F);
		if (M >= 0xB0 && M <= 0xBF)
		{
			const int32 N = M & 0x0F;
			const uint8 Tag = ReadByte();
			FBoltValueArray Fields;
			Fields.Reserve(N);
			for (int32 i = 0; i < N; i++) Fields.Add(ReadValue());
			return FBoltValue::MakeStructure(Tag, MoveTemp(Fields));
		}

		switch (M)
		{
		case 0xC0: return FBoltValue::MakeNull();
		case 0xC1: return FBoltValue::MakeFloat(ReadFloat64BE());
		case 0xC2: return FBoltValue::MakeBool(false);
		case 0xC3: return FBoltValue::MakeBool(true);
		case 0xC8: return FBoltValue::MakeInt(static_cast<int64>(static_cast<int8>(ReadByte())));
		case 0xC9: return FBoltValue::MakeInt(static_cast<int64>(static_cast<int16>(ReadUInt16BE())));
		case 0xCA: return FBoltValue::MakeInt(static_cast<int64>(ReadInt32BE()));
		case 0xCB: return FBoltValue::MakeInt(ReadInt64BE());

		case 0xCC: case 0xCD: case 0xCE:
		{
			const int32 Len = (M == 0xCC) ? ReadByte() : (M == 0xCD) ? ReadUInt16BE() : ReadInt32BE();
			auto V = MakeShared<FBoltValue>();
			V->Type = EBoltValueType::Bytes;
			V->BytesVal.Append(Data.GetData() + Cursor, Len);
			Cursor += Len;
			return V;
		}

		case 0xD0: return FBoltValue::MakeString(ReadStringOfLength(ReadByte()));
		case 0xD1: return FBoltValue::MakeString(ReadStringOfLength(ReadUInt16BE()));
		case 0xD2: return FBoltValue::MakeString(ReadStringOfLength(ReadInt32BE()));

		case 0xD4: return ReadList(ReadByte());
		case 0xD5: return ReadList(ReadUInt16BE());
		case 0xD6: return ReadList(ReadInt32BE());

		case 0xD8: return ReadMap(ReadByte());
		case 0xD9: return ReadMap(ReadUInt16BE());
		case 0xDA: return ReadMap(ReadInt32BE());

		default:
			return FBoltValue::MakeNull();
		}
	}

private:
	FBoltValuePtr ReadList(int32 S)
	{
		FBoltValueArray Items;
		Items.Reserve(S);
		for (int32 i = 0; i < S; i++) Items.Add(ReadValue());
		return FBoltValue::MakeList(MoveTemp(Items));
	}

	FBoltValuePtr ReadMap(int32 S)
	{
		FBoltValueMap Map;
		Map.Reserve(S);
		for (int32 i = 0; i < S; i++)
		{
			auto K = ReadValue();
			Map.Add(K->StringVal, ReadValue());
		}
		return FBoltValue::MakeMap(MoveTemp(Map));
	}

	uint8 ReadByte() { return Data.GetData()[Cursor++]; }

	uint16 ReadUInt16BE()
	{
		const uint8* P = Data.GetData() + Cursor;
		Cursor += 2;
		return (uint16(P[0]) << 8) | P[1];
	}

	int32 ReadInt32BE()
	{
		const uint8* P = Data.GetData() + Cursor;
		Cursor += 4;
		return (int32(P[0]) << 24) | (int32(P[1]) << 16) | (int32(P[2]) << 8) | P[3];
	}

	int64 ReadInt64BE()
	{
		const uint8* P = Data.GetData() + Cursor;
		Cursor += 8;
		int64 R = 0;
		for (int i = 0; i < 8; i++) R = (R << 8) | P[i];
		return R;
	}

	double ReadFloat64BE()
	{
		const uint64 B = static_cast<uint64>(ReadInt64BE());
		double R;
		FMemory::Memcpy(&R, &B, 8);
		return R;
	}

	FString ReadStringOfLength(int32 Len)
	{
		if (Len == 0) return TEXT("");
		FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Data.GetData() + Cursor), Len);
		Cursor += Len;
		return FString(Conv.Length(), Conv.Get());
	}

	const TArray<uint8>& Data;
	int32 Cursor = 0;
};

/** Structural equality of two decoded trees: type, scalar value, and every child, recursively. */
inline bool DeepEqual(const FBoltValuePtr& A, const FBoltValuePtr& B)
{
	if (!A.IsValid() || !B.IsValid()) return A.IsValid() == B.IsValid();
	if (A->Type != B->Type) return false;
	switch (A->Type)
	{
	case EBoltValueType::Null: return true;
	case EBoltValueType::Bool: return A->BoolVal == B->BoolVal;
	case EBoltValueType::Int: return A->IntVal == B->IntVal;
	case EBoltValueType::Float: return A->FloatVal == B->FloatVal;
	case EBoltValueType::String: return A->StringVal == B->StringVal;
	case EBoltValueType::Bytes: return A->BytesVal == B->BytesVal;
	case EBoltValueType::List:
	{
		if (A->ListVal.Num() != B->ListVal.Num()) return false;
		for (int32 i = 0; i < A->ListVal.Num(); ++i)
		{
			if (!DeepEqual(A->ListVal[i], B->ListVal[i])) return false;
		}
		return true;
	}
	case EBoltValueType::Map:
	{
		if (A->MapVal.Num() != B->MapVal.Num()) return false;
		for (const auto& Pair : A->MapVal)
		{
			const FBoltValuePtr* Other = B->MapVal.Find(Pair.Key);
			if (!Other || !DeepEqual(Pair.Value, *Other)) return false;
		}
		return true;
	}
	case EBoltValueType::Structure:
	{
		if (A->StructVal.Tag != B->StructVal.Tag || A->StructVal.Fields.Num() != B->StructVal.Fields.Num()) return false;
		for (int32 i = 0; i < A->StructVal.Fields.Num(); ++i)
		{
			if (!DeepEqual(A->StructVal.Fields[i], B->StructVal.Fields[i])) return false;
		}
		return true;
	}
	}
	return false;
}
} // namespace FantasiaLegacyBolt
