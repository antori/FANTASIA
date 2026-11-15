/**
 * @file BoltPackStream.h
 * @brief Neo4j Bolt PackStream serialization — value types, writer, and reader.
 *
 * Implements the PackStream binary encoding used by the Neo4j Bolt protocol.
 * All Bolt messages are serialized into PackStream before framing, and
 * deserialized back after de-chunking.
 *
 * @see BoltProtocol.h, Neo4jComponent.h
 */

#pragma once

#include "CoreMinimal.h"
#include "FANTASIA.h"

/** Discriminator for the variant value stored in FBoltValue. */
enum class EBoltValueType : uint8
{
	Null, Bool, Int, Float, String, Bytes, List, Map, Structure
};

struct FBoltValue;

/** Shared pointer to a PackStream value (used throughout the Bolt layer). */
using FBoltValuePtr = TSharedPtr<FBoltValue>;

/** Ordered array of PackStream values. */
using FBoltValueArray = TArray<FBoltValuePtr>;

/** String-keyed map of PackStream values. */
using FBoltValueMap = TMap<FString, FBoltValuePtr>;

/** A PackStream structure — a tagged tuple (e.g. Node, Relationship). */
struct FBoltStructure
{
	uint8 Tag = 0;              ///< Structure signature byte.
	FBoltValueArray Fields;     ///< Positional fields within the structure.
};

/**
 * A variant value in the Bolt PackStream encoding.
 *
 * Supports null, bool, int64, double, string, bytes, list, map, and
 * structure types. Provides static factory methods for convenient construction.
 */
struct FANTASIA_API FBoltValue
{
	EBoltValueType Type = EBoltValueType::Null;
	bool BoolVal = false;
	int64 IntVal = 0;
	double FloatVal = 0.0;
	FString StringVal;
	TArray<uint8> BytesVal;
	FBoltValueArray ListVal;
	FBoltValueMap MapVal;
	FBoltStructure StructVal;

	/** @name Factory methods */
	///@{
	static FBoltValuePtr MakeNull() { auto V = MakeShared<FBoltValue>(); return V; }
	static FBoltValuePtr MakeBool(bool B) { auto V = MakeShared<FBoltValue>(); V->Type = EBoltValueType::Bool; V->BoolVal = B; return V; }
	static FBoltValuePtr MakeInt(int64 I) { auto V = MakeShared<FBoltValue>(); V->Type = EBoltValueType::Int; V->IntVal = I; return V; }
	static FBoltValuePtr MakeFloat(double F) { auto V = MakeShared<FBoltValue>(); V->Type = EBoltValueType::Float; V->FloatVal = F; return V; }
	static FBoltValuePtr MakeString(const FString& S) { auto V = MakeShared<FBoltValue>(); V->Type = EBoltValueType::String; V->StringVal = S; return V; }

	static FBoltValuePtr MakeList(FBoltValueArray&& L)
	{
		auto V = MakeShared<FBoltValue>(); V->Type = EBoltValueType::List; V->ListVal = MoveTemp(L); return V;
	}

	static FBoltValuePtr MakeMap(FBoltValueMap&& M)
	{
		auto V = MakeShared<FBoltValue>(); V->Type = EBoltValueType::Map; V->MapVal = MoveTemp(M); return V;
	}

	static FBoltValuePtr MakeStructure(uint8 Tag, FBoltValueArray&& Fields)
	{
		auto V = MakeShared<FBoltValue>(); V->Type = EBoltValueType::Structure; V->StructVal.Tag = Tag; V->StructVal.Fields = MoveTemp(Fields); return V;
	}
	///@}

	/** Convert this value to a human-readable string representation. */
	FString AsString() const;
};

/**
 * Sequential writer for the Bolt PackStream binary encoding.
 *
 * Call the typed Write methods in order, then retrieve the raw bytes via
 * GetBuffer(). Reset() clears the internal buffer for reuse.
 */
class FANTASIA_API FBoltPackStreamWriter
{
public:
	void WriteNull();
	void WriteBool(bool Value);
	void WriteInt(int64 Value);
	void WriteFloat(double Value);
	void WriteString(const FString& Value);
	void WriteBytes(const TArray<uint8>& Value);
	void WriteListHeader(int32 Size);
	void WriteMapHeader(int32 Size);
	void WriteStructureHeader(uint8 Tag, int32 NumFields);

	/** Write a complete FBoltValue tree recursively. */
	void WriteValue(const FBoltValuePtr& Value);

	/** Access the serialized byte buffer. */
	const TArray<uint8>& GetBuffer() const { return Buffer; }

	/** Clear the internal buffer for reuse. */
	void Reset() { Buffer.Empty(); }

private:
	void WriteByte(uint8 B);
	void WriteUInt16BE(uint16 Value);
	void WriteInt16BE(int16 Value);
	void WriteInt32BE(int32 Value);
	void WriteInt64BE(int64 Value);
	void WriteFloat64BE(double Value);
	TArray<uint8> Buffer;
};

/**
 * Sequential reader for the Bolt PackStream binary encoding.
 *
 * Constructed with a raw byte buffer; call ReadValue() repeatedly until
 * HasMore() returns false.
 *
 * The buffer comes off a socket, so nothing in it is trusted. Every read is
 * bounded against the buffer *before* it happens, and a length, an element
 * count or a nesting depth the buffer cannot satisfy is refused rather than
 * clamped: the reader logs the reason once, yields a null value, and reports
 * itself exhausted, so `HasMore()` is false and every later `ReadValue()`
 * refuses too. A refusal inside a nested value propagates outwards as a null
 * value — a partially decoded message is never returned.
 */
class FANTASIA_API FBoltPackStreamReader
{
public:
	/** Construct a reader over @p InData. Does not copy the buffer. */
	FBoltPackStreamReader(const TArray<uint8>& InData);

	/** Read and return the next PackStream value; a null value on refusal. */
	FBoltValuePtr ReadValue();

	/** Returns true if unread bytes remain and the reader has not refused. */
	bool HasMore() const;

	/** Peek at the next marker byte without advancing the cursor. */
	uint8 PeekMarker() const;

private:
	/** Refuse the rest of the buffer: log @p Reason once and exhaust the reader. */
	void Refuse(const FString& Reason);

	/**
	 * True when @p NumBytes more bytes are readable; refuses and returns false
	 * otherwise, negative values included. Called before every read, Reserve and
	 * Append — never after one. A declared element count is bounded by the same
	 * test, because every element costs at least one byte.
	 */
	bool Require(int32 NumBytes);

	/** @p Depth counts enclosing containers; it is a parameter so no path can forget to undo it. */
	FBoltValuePtr ReadValue(int32 Depth);
	FBoltValuePtr ReadMarkedValue(int32 Depth);
	FBoltValuePtr ReadList(int32 Count, int32 Depth);
	FBoltValuePtr ReadMap(int32 Count, int32 Depth);
	FBoltValuePtr ReadStructure(int32 NumFields, int32 Depth);

	uint8 ReadByte();
	uint16 ReadUInt16BE();
	int16 ReadInt16BE();
	int32 ReadInt32BE();
	int64 ReadInt64BE();
	double ReadFloat64BE();
	FString ReadStringOfLength(int32 Length);
	const TArray<uint8>& Data;
	int32 Cursor = 0;
	bool bRefused = false;
};
