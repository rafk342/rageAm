//
// File: vertexdeclaration.h
//
// Copyright (C) 2023 ranstar74. All rights violated.
//
// Part of "Rage Am" Research Project.
//
#pragma once

#include <dxgiformat.h>

#include "dxgi_utils.h"
#include "am/types.h"
#include "rage/grcore/fvf.h"
#include "rage/grcore/effect.h"

namespace rageam::graphics
{
	static constexpr u32 MAX_VERTEX_ELEMENTS = 16;
	static constexpr u32 MAX_SEMANTIC = 8; // Largest semantic index we've got on TEXCOORD ( 0 - 7 )

	enum VertexSemantic : u8 // Fully matches grcVertexSemantic
	{
		POSITION = 0,
		POSITIONT = 1,
		NORMAL = 2,
		BINORMAL = 3,
		TANGENT = 4,
		TEXCOORD = 5,
		BLENDWEIGHT = 6,
		BLENDINDICES = 7,
		COLOR = 8,
	};

	inline VertexSemantic SemanticFromGrc(rage::grcVertexSemantic grcSemantic)
	{
		return VertexSemantic(grcSemantic); // Fully matches so simply cast
	}

	inline ConstString FormatSemanticName(VertexSemantic semantic, int index)
	{
		static char buffer[32];
		sprintf_s(buffer, 32, "%s%i", rage::VertexSemanticName[semantic], index);
		return buffer;
	}

	struct VertexAttribute
	{
		VertexSemantic	Semantic;
		u32				SemanticIndex;
		u32				Offset;
		DXGI_FORMAT		Format;
		u32				SizeInBytes;
	}; using VertexAttributes = FixedList<VertexAttribute, MAX_VERTEX_ELEMENTS>;

	/**
	 * \brief Wraps grcVertexDeclaration in a bit easier format to work with
	 * (for e.g. grcVertexElementDesc stores semantic as string and offset is not present)
	 */
	struct VertexDeclaration
	{
		VertexAttributes			Attributes;
		u32							Stride;
		rage::grcVertexFormatInfo	GrcInfo;	// Cached rage::grc declaration

		VertexDeclaration() = default;
		VertexDeclaration(const rage::grcVertexFormatInfo& vertexInfo)
		{
			Stride = 0;
			GrcInfo = vertexInfo;

			InitFromGrcInfo();
		}
		VertexDeclaration(const rage::grcFvf* fvf)
		{
			Stride = 0;
			GrcInfo = rage::grcVertexDeclaration::CreateFromFvf(*fvf);

			InitFromGrcInfo();
		}

		void ComputeStride()
		{
			Stride = 0;
			for (const VertexAttribute& attribute : Attributes)
				Stride += attribute.SizeInBytes;
		}

		// Fills in stride and attributes from grcVertexFormatInfo
		void InitFromGrcInfo()
		{
			AM_ASSERT(GrcInfo.Decl != nullptr, "VertexDeclaration::InitFromGrcInfo() -> grcVertexDeclaration is NULL!");

			Stride = GrcInfo.Decl->Stride;

			Attributes.Clear();
			for (u32 i = 0; i < GrcInfo.Decl->ElementCount; i++)
			{
				rage::grcVertexElementDesc& desc = GrcInfo.Decl->Elements[i];

				VertexAttribute attr;
				attr.Format = desc.Format;
				attr.SemanticIndex = desc.SemanticIndex;
				attr.Semantic = SemanticFromGrc(rage::GetVertexSemanticFromName(desc.SemanticName));
				attr.Offset = GrcInfo.Decl->GetElementOffset(i);
				attr.SizeInBytes = DXGI::BytesPerPixel(desc.Format);

				Attributes.Add(attr);
			}
		}

		void SetGrcInfo(const rage::grcVertexFormatInfo& vertexFormatInfo)
		{
			GrcInfo = vertexFormatInfo;
			InitFromGrcInfo();
		}

		void FromEffect(rage::grcEffect* effect, bool skinned)
		{
			GrcInfo.FromEffect(effect, skinned);
			InitFromGrcInfo();
		}

		const VertexAttribute* FindAttribute(VertexSemantic semantic, u32 semanticIndex) const
		{
			for (const VertexAttribute& attribute : Attributes)
			{
				if (attribute.Semantic == semantic && attribute.SemanticIndex == semanticIndex)
					return &attribute;
			}
			return nullptr;
		}
	};
}
