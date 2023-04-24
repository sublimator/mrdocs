//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "AdocWriter.hpp"
#include <mrdox/Metadata.hpp>
#include <mrdox/Metadata/Overloads.hpp>
#include <clang/Basic/Specifiers.h>

namespace clang {
namespace mrdox {
namespace adoc {

//------------------------------------------------
//
// AdocWriter
//
//------------------------------------------------

struct AdocWriter::FormalParam
{
    FieldTypeInfo const& I;
    AdocWriter& w;

    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        FormalParam const& t)
    {
        t.w.writeFormalParam(t, os);
        return os;
    }
};

struct AdocWriter::TypeName
{
    TypeInfo const& I;
    AdocWriter& w;

    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        TypeName const& t)
    {
        t.w.writeTypeName(t, os);
        return os;
    }
};

//------------------------------------------------
//
// AdocWriter
//
//------------------------------------------------

AdocWriter::
AdocWriter(
    llvm::raw_ostream& os,
    llvm::raw_fd_ostream* fd_os,
    Corpus const& corpus,
    Reporter& R) noexcept
    : os_(os)
    , fd_os_(fd_os)
    , corpus_(corpus)
    , R_(R)
{
}

//------------------------------------------------

void
AdocWriter::
writeFormalParam(
    FormalParam const& t,
    llvm::raw_ostream& os)
{
    auto const& I = t.I;
    os << I.Type.Name << ' ' << I.Name;
}

auto
AdocWriter::
formalParam(
    FieldTypeInfo const& t) ->
        FormalParam
{
    return FormalParam{ t, *this };
}

//------------------------------------------------

void
AdocWriter::
write(
    RecordInfo const& I)
{
    openSection(I.Name);

    // Brief
    writeBrief(I.javadoc);

    // Synopsis
    openSection("Synopsis");

    // Location
    writeLocation(I);

    // Declaration
    os_ <<
        "\n" <<
        "[,cpp]\n"
        "----\n" <<
        toString(I.TagType) << " " << I.Name;
    if(! I.Bases.empty())
    {
        os_ << "\n    : ";
        writeBase(I.Bases[0]);
        for(std::size_t i = 1; i < I.Bases.size(); ++i)
        {
            os_ << "\n    , ";
            writeBase(I.Bases[i]);
        }
    }
    os_ <<
        ";\n"
        "----\n";
    closeSection();

    // Description
    writeDescription(I.javadoc);

    // Nested Types
    writeNestedTypes(
        "Types",
        I.Children.Typedefs,
        AccessSpecifier::AS_public);

    // Data Members
    writeDataMembers(
        "Data Members",
        I.Members,
        AccessSpecifier::AS_public);

    // Member Functions
    writeFunctionOverloads(
        "Member Functions",
        makeOverloadsSet(corpus_, I.Children,
            AccessSpecifier::AS_public));

    // Data Members (protected)
    writeDataMembers(
        "Protected Data Members",
        I.Members,
        AccessSpecifier::AS_protected);

    // Member Functiosn (protected)
    writeFunctionOverloads(
        "Protected Member Functions",
        makeOverloadsSet(corpus_, I.Children,
                AccessSpecifier::AS_protected));

    // Data Members (private)
    writeDataMembers(
        "Private Data Members",
        I.Members,
        AccessSpecifier::AS_private);

    // Member Functions (private)
    writeFunctionOverloads(
        "Private Member Functions",
        makeOverloadsSet(corpus_, I.Children,
            AccessSpecifier::AS_private));

    closeSection();
}

void
AdocWriter::
write(
    FunctionInfo const& I)
{
    openSection(I.Name);

    // Brief
    writeBrief(I.javadoc);

    // Synopsis
    openSection("Synopsis");

    writeLocation(I);

    os_ <<
        "\n"
        "[,cpp]\n"
        "----\n";
    if(! I.Params.empty())
    {
        os_ <<
            typeName(I.ReturnType) << '\n' <<
            I.Name << "(\n"
            "    " << formalParam(I.Params[0]);
        for(std::size_t i = 1; i < I.Params.size(); ++i)
        {
            os_ << ",\n"
                "    " << formalParam(I.Params[i]);
        }
        os_ << ");\n";
    }
    else
    {
        os_ <<
            typeName(I.ReturnType) << '\n' <<
            I.Name << "();" << "\n";
    }
    os_ <<
        "----\n";
    closeSection();

    // Description
    writeDescription(I.javadoc);

    closeSection();
}

void
AdocWriter::
write(
    TypedefInfo const& I)
{
    openSection(I.Name);

    // Brief
    writeBrief(I.javadoc);

    writeLocation(I);

    // Description
    writeDescription(I.javadoc);

    closeSection();
}

void
AdocWriter::
write(
    EnumInfo const& I)
{
    openSection(I.Name);

    // Brief
    writeBrief(I.javadoc);

    writeLocation(I);

    // Description
    writeDescription(I.javadoc);

    closeSection();
}

//------------------------------------------------

void
AdocWriter::
writeBase(
    BaseRecordInfo const& I)
{
    os_ << clang::getAccessSpelling(I.Access) << " " << I.Name;
}

void
AdocWriter::
writeFunctionOverloads(
    llvm::StringRef sectionName,
    OverloadsSet const& set)
{
    if(set.list.empty())
        return;
    openSection(sectionName);
    os_ <<
        "\n"
        "[,cols=2]\n" <<
        "|===\n" <<
        "|Name |Description\n" <<
        "\n";
    for(auto const& J : set.list)
    {
        os_ <<
            "|`" << J.name << "`\n" <<
            "|";
        if(! J.list.empty())
        {
            for(auto const& K : J.list)
            {
                writeBrief(K->javadoc, false);
                os_ << '\n';
            }
        }
        else
        {
            os_ << '\n';
        }
    }   
    os_ <<
        "|===\n" <<
        "\n";
    closeSection();
}

void
AdocWriter::
writeNestedTypes(
    llvm::StringRef sectionName,
    std::vector<TypedefInfo> const& list,
    AccessSpecifier access)
{
    if(list.empty())
        return;
    auto it = list.begin();
#if 0
    for(;;)
    {
        if(it == list.end())
            return;
        if(it->Access == access)
            break;
        ++it;
    }
#endif
    openSection(sectionName);
    os_ <<
        "\n"
        "[,cols=2]\n" <<
        "|===\n" <<
        "|Name |Description\n" <<
        "\n";
    for(;it != list.end(); ++it)
    {
#if 0
        if(it->Access != access)
            continue;
#endif
        os_ <<
            "|`" << it->Name << "`\n" <<
            "|";
        writeBrief(it->javadoc, false);
        os_ << '\n';
    }   
    os_ <<
        "|===\n" <<
        "\n";
    closeSection();
}

void
AdocWriter::
writeDataMembers(
    llvm::StringRef sectionName,
    llvm::SmallVectorImpl<MemberTypeInfo> const& list,
    AccessSpecifier access)
{
    if(list.empty())
        return;

    auto it = list.begin();
    for(;;)
    {
        if(it == list.end())
            return;
        if(it->Access == access)
            break;
        ++it;
    }
    openSection(sectionName);
    os_ <<
        "\n"
        "[,cols=2]\n" <<
        "|===\n" <<
        "|Name |Description\n" <<
        "\n";
    for(;it != list.end(); ++it)
    {
        if(it->Access != access)
            continue;
        os_ <<
            "|`" << it->Name << "`\n" <<
            "|";
        writeBrief(it->javadoc, false);
        os_ << '\n';
    }   
    os_ <<
        "|===\n" <<
        "\n";
    closeSection();
}

//------------------------------------------------

void
AdocWriter::
writeBrief(
    llvm::Optional<Javadoc> const& javadoc,
    bool withNewline)
{
    if(! javadoc.has_value())
        return;
    auto const node = javadoc->getBrief();
    if(! node)
        return;
    if(node->empty())
        return;
    if(withNewline)
        os_ << '\n';
    writeNode(*node);
}

void
AdocWriter::
writeDescription(
    llvm::Optional<Javadoc> const& javadoc)
{
    if(! javadoc.has_value())
        return;

    //os_ << '\n';
    openSection("Description");
    os_ << '\n';
    writeNodes(javadoc->getBlocks());
    closeSection();
}

void
AdocWriter::
writeLocation(
    SymbolInfo const& I)
{
    Location const* loc = nullptr;
    if(I.DefLoc.has_value())
        loc = &*I.DefLoc;
    else if(! I.Loc.empty())
        loc = &I.Loc[0];

    std::string url;
    //--------------------------------------------
    llvm::raw_string_ostream os(url);

    // relative href
#if 1
    os << "link:" << loc->Filename;
#else
    os << "file:///" << loc->Filename;
#endif
    //--------------------------------------------

    switch(I.IT)
    {
    default:
    case InfoType::IT_function:
        os_ <<
            "\n"
            "Declared in " << url <<
            "[" << loc->Filename << "]" <<
            "\n";
        break;
    case InfoType::IT_record:
        os_ <<
            "\n"
            "`#include <" << url <<
            "[" << loc->Filename << "]" <<
            ">`\n";
        break;
    }
}

//------------------------------------------------

template<class T>
void
AdocWriter::
writeNodes(
    List<T> const& list)
{
    if(list.empty())
        return;
    for(Javadoc::Node const& node : list)
        writeNode(node);
}

void
AdocWriter::
writeNode(
    Javadoc::Node const& node)
{
    switch(node.kind)
    {
    case Javadoc::Kind::text:
        writeNode(static_cast<Javadoc::Text const&>(node));
        return;
    case Javadoc::Kind::styled:
        writeNode(static_cast<Javadoc::StyledText const&>(node));
        return;
#if 0
    case Javadoc::Node::block:
        writeNode(static_cast<Javadoc::Block const&>(node));
        return;
#endif
    case Javadoc::Kind::brief:
    case Javadoc::Kind::paragraph:
        writeNode(static_cast<Javadoc::Paragraph const&>(node));
        return;
    case Javadoc::Kind::admonition:
        writeNode(static_cast<Javadoc::Admonition const&>(node));
        return;
    case Javadoc::Kind::code:
        writeNode(static_cast<Javadoc::Code const&>(node));
        return;
    case Javadoc::Kind::param:
        writeNode(static_cast<Javadoc::Param const&>(node));
        return;
    case Javadoc::Kind::tparam:
        writeNode(static_cast<Javadoc::TParam const&>(node));
        return;
    case Javadoc::Kind::returns:
        writeNode(static_cast<Javadoc::Returns const&>(node));
        return;
    default:
        llvm_unreachable("unknown kind");
    }
}

void
AdocWriter::
writeNode(
    Javadoc::Text const& node)
{
    os_ << node.string << '\n';
}

void
AdocWriter::
writeNode(
    Javadoc::StyledText const& node)
{
    switch(node.style)
    {
    case Javadoc::Style::bold:
        os_ << '*' << node.string << "*\n";
        break;
    case Javadoc::Style::mono:
        os_ << '`' << node.string << "`\n";
        break;
    case Javadoc::Style::italic:
        os_ << '_' << node.string << "_\n";
        break;
    default:
        os_ << node.string << '\n';
        break;
    }
}

void
AdocWriter::
writeNode(
    Javadoc::Paragraph const& node)
{
    writeNodes(node.children);
}

void
AdocWriter::
writeNode(
    Javadoc::Admonition const& node)
{
    writeNodes(node.children);
}

void
AdocWriter::
writeNode(
    Javadoc::Code const& node)
{
    os_ <<
        "[,cpp]\n"
        "----\n";
    writeNodes(node.children);
    os_ <<
        "----\n";
}

void
AdocWriter::
writeNode(
    Javadoc::Param const& node)
{
}

void
AdocWriter::
writeNode(
    Javadoc::TParam const& node)
{
}

void
AdocWriter::
writeNode(
    Javadoc::Returns const& node)
{
}

//------------------------------------------------

void
AdocWriter::
writeTypeName(
    TypeName const& t,
    llvm::raw_ostream& os)
{
    if(t.I.Type.id == globalNamespaceID)
    {
        os_ << t.I.Type.Name;
        return;
    }
    if(corpus_.exists(t.I.Type.id))
    {
        auto const& J = corpus_.get<RecordInfo>(t.I.Type.id);
        // VFALCO add namespace qualifiers if I is in
        //        a different namesapce
        os_ << J.Name << "::" << J.Name;
        return;
    }
    auto const& T = t.I.Type;
    os_ << T.Name << "::" << T.Name;
}

auto
AdocWriter::
typeName(
    TypeInfo const& t) ->
        TypeName
{
    return TypeName{ t, *this };
}

//------------------------------------------------

void
AdocWriter::
openSection(
    llvm::StringRef name)
{
    sect_.level++;
    if(sect_.level <= 6)
        sect_.markup.push_back('=');
    os_ <<
        "\n" <<
        sect_.markup << ' ' << name << "\n";
}

void
AdocWriter::
closeSection()
{
    Assert(sect_.level > 0);
    if(sect_.level <= 6)
        sect_.markup.pop_back();
    sect_.level--;
}

//------------------------------------------------

llvm::StringRef
AdocWriter::
toString(TagTypeKind k) noexcept
{
    switch(k)
    {
    case TagTypeKind::TTK_Struct: return "struct";
    case TagTypeKind::TTK_Interface: return "__interface";
    case TagTypeKind::TTK_Union: return "union";
    case TagTypeKind::TTK_Class: return "class";
    case TagTypeKind::TTK_Enum: return "enum";
    default:
        llvm_unreachable("unknown TagTypeKind");
    }
}

} // adoc
} // mrdox
} // clang
