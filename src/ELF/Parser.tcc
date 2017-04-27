/* Copyright 2017 R. Thomas
 * Copyright 2017 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "easylogging++.h"

namespace LIEF {
namespace ELF {
template<typename ELF_T>
void Parser::parse_binary(void) {
  LOG(DEBUG) << "Start parsing";
  // Parse header
  // ============
  this->parse_header<ELF_T>();

  // Parse Sections
  // ==============
  if (this->binary_->header_.section_headers_offset() > 0) {
    this->parse_sections<ELF_T>();
  } else {
    LOG(WARNING) << "The current binary doesn't have a section header";
  }


  // Parse segments
  // ==============
  if (this->binary_->header_.program_headers_offset() > 0) {
    this->parse_segments<ELF_T>();
  } else {
    LOG(WARNING) << "Binary doesn't have a program header";
  }

  // Parse Dynamic elements
  // ======================

  // Find the dynamic Segment
  auto&& it_segment_dynamic = std::find_if(
      std::begin(this->binary_->segments_),
      std::end(this->binary_->segments_),
      [] (const Segment* segment) {
        return segment != nullptr and segment->type() == SEGMENT_TYPES::PT_DYNAMIC;
      });

  if (it_segment_dynamic != std::end(this->binary_->segments_)) {

    const uint64_t offset = (*it_segment_dynamic)->file_offset();
    const uint64_t size   = (*it_segment_dynamic)->physical_size();

    this->parse_dynamic_entries<ELF_T>(offset, size);
  }


  // Parse dynamic symbols
  // =====================
  auto&& it_dynamic_symbol_table = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_SYMTAB;
      });

  auto&& it_dynamic_symbol_size = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_SYMENT;
      });

  if (it_dynamic_symbol_table != std::end(this->binary_->dynamic_entries_) and
      it_dynamic_symbol_size != std::end(this->binary_->dynamic_entries_)) {
    const uint64_t virtual_address = (*it_dynamic_symbol_table)->value();
    const uint64_t size            = (*it_dynamic_symbol_size)->value();
    try {
      const uint64_t offset = this->binary_->virtual_address_to_offset(virtual_address);
      this->parse_dynamic_symbols<ELF_T>(offset, size);
    } catch (const LIEF::exception&) {
    }
  }

  // Parse dynamic relocations
  // =========================

  // RELA
  // ----
  auto&& it_dynamic_relocations = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_RELA;
      });

  auto&& it_dynamic_relocations_size = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_RELASZ;
      });

  if (it_dynamic_relocations != std::end(this->binary_->dynamic_entries_) and
      it_dynamic_relocations_size != std::end(this->binary_->dynamic_entries_)) {
    const uint64_t virtual_address = (*it_dynamic_relocations)->value();
    const uint64_t size            = (*it_dynamic_relocations_size)->value();
    try {
      uint64_t offset = this->binary_->virtual_address_to_offset(virtual_address);
      this->parse_dynamic_relocations<ELF_T>(offset, size, true);
    } catch (const LIEF::exception&) {
    }
  }


  // REL
  // ---
  it_dynamic_relocations = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_REL;
      });

  it_dynamic_relocations_size = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_RELSZ;
      });

  if (it_dynamic_relocations != std::end(this->binary_->dynamic_entries_) and
      it_dynamic_relocations_size != std::end(this->binary_->dynamic_entries_)) {
    const uint64_t virtual_address = (*it_dynamic_relocations)->value();
    const uint64_t size            = (*it_dynamic_relocations_size)->value();
    try {
      const uint64_t offset = this->binary_->virtual_address_to_offset(virtual_address);
      this->parse_dynamic_relocations<ELF_T>(offset, size, false);
    } catch (const LIEF::exception&) {

    }

  }

  // Parse PLT/GOT Relocations
  // ==========================
  auto&& it_pltgot_relocations = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_JMPREL;
      });

  auto&& it_pltgot_relocations_size = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_PLTRELSZ;
      });

  auto&& it_pltgot_relocations_type = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_PLTREL;
      });

  if (it_pltgot_relocations != std::end(this->binary_->dynamic_entries_) and
      it_pltgot_relocations_size != std::end(this->binary_->dynamic_entries_)) {
    const uint64_t virtual_address = (*it_pltgot_relocations)->value();
    const uint64_t size            = (*it_pltgot_relocations_size)->value();
    DYNAMIC_TAGS type;
    if (it_pltgot_relocations_type != std::end(this->binary_->dynamic_entries_)) {
      type = static_cast<DYNAMIC_TAGS>((*it_pltgot_relocations_type)->value());
    } else {
      // Try to guess: We assume that on ELF64 -> DT_RELA and on ELF32 -> DT_REL
      if (std::is_same<ELF_T, ELF64>::value) {
        type = DYNAMIC_TAGS::DT_RELA;
      } else {
        type = DYNAMIC_TAGS::DT_REL;
      }
    }

    try {
      const uint64_t offset = this->binary_->virtual_address_to_offset(virtual_address);
      this->parse_pltgot_relocations<ELF_T>(
        offset, size,
        type == DYNAMIC_TAGS::DT_RELA ? true : false);
    } catch (const LIEF::exception&) {

    }


  }

  // Parse Symbol Version
  // ====================
  auto&& it_symbol_versions = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_VERSYM;
      });

  if (it_symbol_versions != std::end(this->binary_->dynamic_entries_)) {
    const uint64_t virtual_address = (*it_symbol_versions)->value();
    try {
      uint64_t offset = this->binary_->virtual_address_to_offset(virtual_address);
      this->parse_symbol_version(offset);
    } catch (const LIEF::exception&) {

    }

  }

  // Parse Symbol Version Requirement
  // ================================
  auto&& it_symbol_version_requirement = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_VERNEED;
      });

  auto&& it_symbol_version_requirement_size = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_VERNEEDNUM;
      });

  if (it_symbol_version_requirement != std::end(this->binary_->dynamic_entries_) and
      it_symbol_version_requirement_size != std::end(this->binary_->dynamic_entries_)) {
    const uint64_t virtual_address = (*it_symbol_version_requirement)->value();
    const uint32_t size            = static_cast<uint32_t>((*it_symbol_version_requirement_size)->value());
    try {
      const uint64_t offset = this->binary_->virtual_address_to_offset(virtual_address);
      this->parse_symbol_version_requirement<ELF_T>(offset, size);
    } catch (const LIEF::exception& e) {
      LOG(WARNING) << e.what();
    }

  }

  // Parse Symbol Version Definition
  // ===============================
  auto&& it_symbol_version_definition = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_VERDEF;
      });

  auto&& it_symbol_version_definition_size = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_VERDEFNUM;
      });

  if (it_symbol_version_definition != std::end(this->binary_->dynamic_entries_) and
      it_symbol_version_definition_size != std::end(this->binary_->dynamic_entries_)) {
    const uint64_t virtual_address = (*it_symbol_version_definition)->value();
    const uint32_t size            = static_cast<uint32_t>((*it_symbol_version_definition_size)->value());
    try {
      const uint64_t offset = this->binary_->virtual_address_to_offset(virtual_address);
      this->parse_symbol_version_definition<ELF_T>(offset, size);
    } catch (const LIEF::exception&) {

    }

  }


  // Parse static symbols
  // ====================
  auto&& it_symtab_section = std::find_if(
      std::begin(this->binary_->sections_),
      std::end(this->binary_->sections_),
      [] (const Section* section)
      {
        return section != nullptr and section->type() == SECTION_TYPES::SHT_SYMTAB;
      });

  if (it_symtab_section != std::end(this->binary_->sections_)) {
    const Section* section = *it_symtab_section;
    uint32_t nb_entries = 0;
    if (this->type_ == ELFCLASS32) {
      nb_entries = static_cast<uint32_t>((section->size() / sizeof(Elf32_Sym)));
    } else {
      nb_entries = static_cast<uint32_t>((section->size() / sizeof(Elf64_Sym)));
    }

    if (section->link() == 0 or section->link() >= this->binary_->sections_.size()) {
      LOG(WARNING) << "section->link() is not valid !";
    } else {
      // We should have:
      // nb_entries == section->information())
      // but lots of compiler not respect this rule
      this->parse_static_symbols<ELF_T>(
          section->file_offset(),
          nb_entries,
          this->binary_->sections_[section->link()]);
    }
  }

  // Parse Symbols's hash
  // ====================

  auto&& it_symbol_hash = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_HASH;
      });

  auto&& it_symbol_gnu_hash = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry) {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_GNU_HASH;
      });

  if (it_symbol_hash != std::end(this->binary_->dynamic_entries_)) {
    try {
      //const uint64_t symbol_hash_offset = this->binary_->virtual_address_to_offset((*itSymbolHash)->value());
    } catch (const conversion_error&) {
    } catch (const corrupted& e) {
      LOG(WARNING) << e.what();
    }
  }


  if (it_symbol_gnu_hash != std::end(this->binary_->dynamic_entries_)) {
    try {
      const uint64_t symbol_gnu_hash_offset = this->binary_->virtual_address_to_offset((*it_symbol_gnu_hash)->value());
      this->parse_symbol_gnu_hash<ELF_T>(symbol_gnu_hash_offset);
    } catch (const conversion_error&) {
    } catch (const corrupted& e) {
      LOG(WARNING) << e.what();
    }
  }

  // Try to parse using sections
  //for (const std::shared_ptr<Section>& section : this->binary_->sections_) {
  //  if (section->name() == ".rela.plt" and
  //      (section->type() == SECTION_TYPES::SHT_RELA or
  //       section->type() == SECTION_TYPES::SHT_REL)) {

  //    this->parse_pltgot_relocations<ELF_T>(
  //        section->file_offset(),
  //        section->size(),
  //        section->type() == SECTION_TYPES::SHT_RELA);
  //  }

  //  if (section->name() == ".rela.dyn" and
  //      (section->type() == SECTION_TYPES::SHT_RELA or
  //       section->type() == SECTION_TYPES::SHT_REL)) {
  //    this->parse_dynamic_relocations<ELF_T>(
  //        section->virtual_address(),
  //        section->size(),
  //        section->type() == SECTION_TYPES::SHT_RELA);
  //  }
  //}

  this->link_symbol_version();
}


template<typename ELF_T>
void Parser::parse_header(void) {
  using Elf_Ehdr = typename ELF_T::Elf_Ehdr;

  LOG(DEBUG) << "[+] Parsing Header";
  try {
    this->binary_->header_ = {reinterpret_cast<const Elf_Ehdr*>(
        this->stream_->read(0, sizeof(Elf_Ehdr)))};
  } catch (const read_out_of_bound&) {
    throw corrupted("Header corrupted");
  }
}


template<typename ELF_T>
uint32_t Parser::get_numberof_dynamic_symbols(void) const {

  if (this->binary_->has_dynamic_entry(DYNAMIC_TAGS::DT_HASH)) {
    return this->nb_dynsym_sysv_hash<ELF_T>();
  }

  if (this->binary_->has_dynamic_entry(DYNAMIC_TAGS::DT_GNU_HASH)) {
    return this->nb_dynsym_gnu_hash<ELF_T>();
  }

  throw not_supported("Unable to determine the number of dynamic symbols");
}


template<typename ELF_T>
uint32_t Parser::nb_dynsym_sysv_hash(void) const {
  const DynamicEntry& dyn_hash = this->binary_->dynamic_entry_from_tag(DYNAMIC_TAGS::DT_HASH);
  const uint64_t offset = this->binary_->virtual_address_to_offset(dyn_hash.value());

  return 0;
}

template<typename ELF_T>
uint32_t Parser::nb_dynsym_gnu_hash(void) const {
  using uint__ = typename ELF_T::uint;

  const DynamicEntry& dyn_hash = this->binary_->dynamic_entry_from_tag(DYNAMIC_TAGS::DT_GNU_HASH);
  const uint64_t offset = this->binary_->virtual_address_to_offset(dyn_hash.value());

  uint64_t current_offset = offset;

  const uint32_t* header = reinterpret_cast<const uint32_t*>(
      this->stream_->read(current_offset, 4 * sizeof(uint32_t)));

  current_offset += 4 * sizeof(uint32_t);

  const uint32_t nbuckets  = header[0];
  const uint32_t symndx    = header[1];
  const uint32_t maskwords = header[2];
  const uint32_t shift2    = header[3];

  if (maskwords & (maskwords - 1)) {
    LOG(WARNING) << "maskwords is not a power of 2";
  }

  std::vector<uint64_t> bloom_filters;
  try {
    bloom_filters.resize(maskwords);

    for (size_t i = 0; i < maskwords; ++i) {
      bloom_filters[i] = this->stream_->read_integer<uint__>(current_offset);
      current_offset += sizeof(uint__);
    }
  }
  catch (const read_out_of_bound&) {
    throw corrupted("GNU Hash, maskwords corrupted");
  }
  catch (const std::bad_alloc&) {
    throw corrupted("GNU Hash, maskwords corrupted");
  }

  std::vector<uint32_t> buckets;
  buckets.reserve(std::min<uint32_t>(nbuckets, 400));
  try {
    const uint32_t* hash_buckets = reinterpret_cast<const uint32_t*>(
        this->stream_->read(current_offset, nbuckets * sizeof(uint32_t)));
    current_offset += nbuckets * sizeof(uint32_t);

    buckets = {hash_buckets, hash_buckets + nbuckets};
  } catch (const read_out_of_bound&) {
    throw corrupted("GNU Hash, hash_buckets corrupted");
  }
  uint32_t nb_symbols = *std::max_element(std::begin(buckets), std::end(buckets));
  nb_symbols = std::max(nb_symbols, symndx);

  const uint32_t* hash_values = reinterpret_cast<const uint32_t*>(
      this->stream_->read(current_offset, nb_symbols * sizeof(uint32_t)));


  // "It is set to 1 when a symbol is the last symbol in a given hash chain"
  while (((*hash_values) & 1) == 0) {
    ++nb_symbols;
    ++hash_values;
  }

  return ++nb_symbols;


}

template<typename ELF_T>
void Parser::parse_sections(void) {
  using Elf_Shdr = typename ELF_T::Elf_Shdr;
  LOG(DEBUG) << "[+] Parsing Section";

  const uint64_t headers_offset    = this->binary_->header_.section_headers_offset();
  const uint32_t numberof_sections = this->binary_->header_.numberof_sections();
  const Elf_Shdr* section_headers = reinterpret_cast<const Elf_Shdr*>(
      this->stream_->read(
        headers_offset,
        numberof_sections * sizeof(Elf_Shdr)));

  for (size_t i = 0; i < numberof_sections; ++i) {

    LOG(DEBUG) << "\t Parsing section " << std::dec << i;
    const Elf_Shdr* hdr = &(section_headers[i]);
    Section* section = new Section{hdr};
    section->datahandler_ = this->binary_->datahandler_;

    // Only if it contains data (with bits)
    if (section->type() != SECTION_TYPES::SHT_NOBITS) {
      const uint64_t offset_to_content   = section->file_offset();
      const uint64_t size                = section->size();
      try {
        const uint8_t* content = static_cast<const uint8_t*>(
            this->stream_->read(offset_to_content, size));
        section->content({content, content + size});
      } catch (const LIEF::read_out_of_bound&) {
        LOG(WARNING) << "Section's file offset and/or section's size is corrupted";
      } catch (const std::bad_alloc&) {
        LOG(WARNING) << "Section's file offset and/or section's size is corrupted";
      }
    } else { //Create a node which will hold nothing
      DataHandler::Node empty_node{
          section->file_offset(),
          section->size(),
          DataHandler::Node::SECTION};
      this->binary_->datahandler_->add_node(empty_node);
    }
    this->binary_->sections_.push_back(section);
  }

  // Parse name
  if (this->binary_->header_.section_name_table_idx() < this->binary_->sections_.size()) {
    const size_t section_string_index = this->binary_->header_.section_name_table_idx();
    const Section* string_section = this->binary_->sections_[section_string_index];
    for (Section* section : this->binary_->sections_) {
      try {
        section->name({this->stream_->read_string(
              string_section->file_offset() + section->name_idx())});
      } catch (const LIEF::read_out_of_bound&) {
        LOG(WARNING) << "Section's name is corrupted";
      }
    }
  } else {
    LOG(WARNING) << "Unable to fetch the Name string section";
  }
}

template<typename ELF_T>
void Parser::parse_segments(void) {
  using Elf_Phdr = typename ELF_T::Elf_Phdr;

  LOG(DEBUG) << "[+] Parse Segments";
  const uint64_t segment_headers_offset = this->binary_->get_header().program_headers_offset();
  const uint32_t nbof_segments          = this->binary_->get_header().numberof_segments();

  const Elf_Phdr* segment_headers = reinterpret_cast<const Elf_Phdr*>(
      this->stream_->read(segment_headers_offset, nbof_segments * sizeof(Elf_Phdr)));

  auto check_section_in_segment =
    [] (const Section* section, const Segment* segment) {
      return section->virtual_address() >= segment->virtual_address() and
        (section->virtual_address() + section->size()) <=
        (segment->virtual_address() + segment->virtual_size());
    };

  for (size_t i = 0; i < nbof_segments; ++i) {
    Segment* segment = new Segment{&segment_headers[i]};
    segment->datahandler_ = this->binary_->datahandler_;
    // If if a section is in the current segment

    if (segment->physical_size() > 0) {
      const uint64_t offset_to_content   = segment->file_offset();
      const uint64_t size                = segment->physical_size();
      try {
        const uint8_t* content = static_cast<const uint8_t*>(
            this->stream_->read(offset_to_content, size));
        segment->content({content, content + size});

      } catch (const LIEF::read_out_of_bound&) {
        LOG(WARNING) << "Segment's file offset and/or segment's size is corrupted";
      } catch(const std::bad_alloc&) {
        LOG(WARNING) << "Segment's file offset and/or segment's size is corrupted";
      }
    }

    for (Section* section : this->binary_->sections_) {
      if (check_section_in_segment(section, segment)) {
        section->segments_.push_back(segment);
        segment->sections_.push_back(section);
      }
    }
    this->binary_->segments_.push_back(segment);
  }

}



template<typename ELF_T>
void Parser::parse_dynamic_relocations(uint64_t relocations_offset, uint64_t size, bool isRela) {
  LOG(DEBUG) << "[+] Parsing dynamic relocations";
  using Elf_Rela = typename ELF_T::Elf_Rela;
  using Elf_Rel  = typename ELF_T::Elf_Rel;

  // Already parsed
  if (this->binary_->dynamic_relocations_.size() > 0) {
    return;
  }

  const uint8_t shift = std::is_same<ELF_T, ELF32>::value ? 8 : 32;

  if (isRela) {
    const uint32_t nb_entries = static_cast<uint32_t>(size / sizeof(Elf_Rela));

    const Elf_Rela* relocEntry = reinterpret_cast<const Elf_Rela*>(
        this->stream_->read(relocations_offset, nb_entries * sizeof(Elf_Rela)));

    for (uint32_t i = 0; i < nb_entries; ++i) {
      Relocation* reloc = new Relocation{relocEntry};
      reloc->architecture_ = this->binary_->get_header().machine_type();
      const uint32_t idx =  static_cast<uint32_t>(relocEntry->r_info >> shift);
      if (idx < this->binary_->dynamic_symbols_.size()) {
        reloc->symbol_ = this->binary_->dynamic_symbols_[idx];
      } else {
        LOG(WARNING) << "Unable to find the symbol associated with the relocation (idx: "
                     << std::dec << idx << ")" << std::endl
                     << reloc;
      }

      this->binary_->dynamic_relocations_.push_back(reloc);
      relocEntry++;
    }
  } else {
    const uint32_t nb_entries = static_cast<uint32_t>(size / sizeof(Elf_Rel));

    const Elf_Rel* relocEntry = reinterpret_cast<const Elf_Rel*>(
       this->stream_->read(relocations_offset, nb_entries * sizeof(Elf_Rel)));

    for (uint32_t i = 0; i < nb_entries; ++i) {
      Relocation* reloc = new Relocation{relocEntry};
      reloc->architecture_ = this->binary_->get_header().machine_type();
      uint32_t idx =  static_cast<uint32_t>(relocEntry->r_info >> shift);
      if (idx < this->binary_->dynamic_symbols_.size()) {
        reloc->symbol_ = this->binary_->dynamic_symbols_[idx];
      } else {
        LOG(WARNING) << "Unable to find the symbol associated with the relocation (idx: "
                     << std::dec << idx << ")" << std::endl
                     << reloc;
      }

      this->binary_->dynamic_relocations_.push_back(reloc);
      relocEntry++;
    }
  }
} // build_dynamic_reclocations



template<typename ELF_T>
void Parser::parse_static_symbols(uint64_t offset, uint32_t nbSymbols, const Section* string_section) {

  using Elf_Sym = typename ELF_T::Elf_Sym;
  LOG(DEBUG) << "[+] Parsing static symbols";

  const Elf_Sym* symbol_headers = reinterpret_cast<const Elf_Sym*>(
      this->stream_->read(offset, nbSymbols * sizeof(Elf_Sym)));

  for (uint32_t i = 0; i < nbSymbols; ++i) {
    Symbol* symbol = new Symbol{&symbol_headers[i]};
    try {
      std::string symbol_name = {this->stream_->read_string(
          string_section->file_offset() + symbol_headers[i].st_name)};
      symbol->name(symbol_name);
    } catch (const LIEF::read_out_of_bound& e) {
      LOG(WARNING) << e.what();
    }
    this->binary_->static_symbols_.push_back(symbol);
  }
} // build_static_symbols


template<typename ELF_T>
void Parser::parse_dynamic_symbols(uint64_t offset, uint64_t size) {
  using Elf_Sym = typename ELF_T::Elf_Sym;
  LOG(DEBUG) << "[+] Parsing dynamics symbols";

  uint32_t nb_symbols = this->get_numberof_dynamic_symbols<ELF_T>();

  const uint64_t dynamic_symbols_offset = offset;
  const uint64_t string_offset = this->get_dynamic_string_table();

  const Elf_Sym* symbol_headers = reinterpret_cast<const Elf_Sym*>(
      this->stream_->read(dynamic_symbols_offset, nb_symbols * sizeof(Elf_Sym)));

  if (string_offset == 0) {
    LOG(WARNING) << "Unable to find the .dynstr section";
  } else {
    for (size_t i = 0; i < nb_symbols; ++i) {
    //while (symbolHdr->st_other == 0 and (idx++) < maxSymbols) { // Check: Could be wrong ?
      Symbol* symbol = new Symbol{symbol_headers};

      if (symbol_headers->st_name > 0) {
        std::string name{
          this->stream_->read_string(string_offset + symbol_headers->st_name)};
        symbol->name(name);
      }

      this->binary_->dynamic_symbols_.push_back(symbol);
      symbol_headers++;
    }
  }
} // build_dynamic_sybols




template<typename ELF_T>
void Parser::parse_dynamic_entries(uint64_t offset, uint64_t size) {
  using Elf_Dyn = typename ELF_T::Elf_Dyn;
  using uint__  = typename ELF_T::uint;
  LOG(DEBUG) << "[+] Parsing dynamic section";

  const uint64_t nb_entries = size / sizeof(Elf_Dyn);

  LOG(DEBUG) << "Size of the dynamic section: 0x" << std::hex << size;
  LOG(DEBUG) << "offset of the dynamic section: 0x" << std::hex << offset;
  LOG(DEBUG) << "Nb of entrie in DynSec = " << std::dec << nb_entries;

  uint64_t dynamic_string_offset = 0;
  try {
    dynamic_string_offset = this->get_dynamic_string_table();
  } catch (const std::exception&) {
    LOG(WARNING) << "Unable to fetch dynamic string table";
  }
  const Elf_Dyn* entries = reinterpret_cast<const Elf_Dyn*>(
      this->stream_->read(offset, nb_entries * sizeof(Elf_Dyn)));

  for (size_t dynIdx = 0; dynIdx < nb_entries; ++dynIdx) {
    const Elf_Dyn* entry = &entries[dynIdx];

    DynamicEntry *dynamic_entry = nullptr;
    switch (static_cast<DYNAMIC_TAGS>(entry->d_tag)) {
      case DYNAMIC_TAGS::DT_NEEDED :
        {
          dynamic_entry = new DynamicEntryLibrary{entry};
          if (dynamic_string_offset == 0) {
            LOG(WARNING) << "Unable to find the .dynstr section";
          } else {
            std::string library_name = {
              this->stream_->read_string(dynamic_string_offset + dynamic_entry->value())};
            dynamic_entry->name(library_name);
          }
          break;
        }

      case DYNAMIC_TAGS::DT_SONAME :
        {

          dynamic_entry = new DynamicSharedObject{entry};

          if (dynamic_string_offset == 0) {
            LOG(WARNING) << "Unable to find the .dynstr section";
          } else {
            std::string sharename = {
              this->stream_->read_string(dynamic_string_offset + dynamic_entry->value())};
            dynamic_entry->name(sharename);
          }
          break;
        }

      case DYNAMIC_TAGS::DT_RPATH:
        {

          dynamic_entry = new DynamicEntryRpath{entry};

          if (dynamic_string_offset == 0) {
            LOG(WARNING) << "Unable to find the .dynstr section";
          } else {
            std::string name = {
              this->stream_->read_string(dynamic_string_offset + dynamic_entry->value())};
            dynamic_entry->name(name);
          }
          break;
        }

      case DYNAMIC_TAGS::DT_RUNPATH:
        {

          dynamic_entry = new DynamicEntryRunPath{entry};

          if (dynamic_string_offset == 0) {
            LOG(WARNING) << "Unable to find the .dynstr section";
          } else {
            std::string name = {
              this->stream_->read_string(dynamic_string_offset + dynamic_entry->value())};
            dynamic_entry->name(name);
          }
          break;
        }


      case DYNAMIC_TAGS::DT_SYMTAB :
        {
          dynamic_entry = new DynamicEntry{entry};
          break;
        }

      case DYNAMIC_TAGS::DT_SYMENT:
        {
          dynamic_entry = new DynamicEntry{entry};
          break;
        }

      case DYNAMIC_TAGS::DT_RELA:
        {
          dynamic_entry = new DynamicEntry{entry};
          break;
        }

      case DYNAMIC_TAGS::DT_RELASZ:
        {
          dynamic_entry = new DynamicEntry{entry};
          break;
        }

        // Relocations 'Elf64Rel'
      case DYNAMIC_TAGS::DT_REL:
        {
          dynamic_entry = new DynamicEntry{entry};
          break;
        }

      case DYNAMIC_TAGS::DT_RELSZ:
        {
          dynamic_entry = new DynamicEntry{entry};
          break;
        }

        // Plt relocations
      case DYNAMIC_TAGS::DT_JMPREL:
        {
          dynamic_entry = new DynamicEntry{entry};
          break;
        }

      case DYNAMIC_TAGS::DT_PLTRELSZ:
        {
          dynamic_entry = new DynamicEntry{entry};
          break;
        }

      case DYNAMIC_TAGS::DT_PLTREL:
        {
          dynamic_entry = new DynamicEntry{entry};
          break;
        }

      case DYNAMIC_TAGS::DT_VERSYM:
        {
          dynamic_entry = new DynamicEntry{entry};
          break;
        }

      case DYNAMIC_TAGS::DT_VERNEED:
        {
          dynamic_entry = new DynamicEntry{entry};
          break;
        }

      case DYNAMIC_TAGS::DT_VERNEEDNUM:
        {
          dynamic_entry = new DynamicEntry{entry};
          break;
        }

      case DYNAMIC_TAGS::DT_VERDEF:
        {
          dynamic_entry = new DynamicEntry{entry};
          break;
        }

      case DYNAMIC_TAGS::DT_VERDEFNUM:
        {
          dynamic_entry = new DynamicEntry{entry};
          break;
        }

      case DYNAMIC_TAGS::DT_FINI_ARRAY:
      case DYNAMIC_TAGS::DT_INIT_ARRAY:
      case DYNAMIC_TAGS::DT_PREINIT_ARRAY:
        {
          dynamic_entry = new DynamicEntryArray{entry};
          break;
        }

      default:
        {
          dynamic_entry = new DynamicEntry{entry};
        }
    }

    if (dynamic_entry != nullptr) {
      this->binary_->dynamic_entries_.push_back(dynamic_entry);
    } else {
      LOG(WARNING) << "dynamic_entry is nullptr !";
    }

  }

  // Check for INIT array
  // ====================
  auto&& it_dt_initarray = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry)
      {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_INIT_ARRAY;
      });

  if (it_dt_initarray != std::end(this->binary_->dynamic_entries_)) {

    auto&& it_dt_initarray_size = std::find_if(
        std::begin(this->binary_->dynamic_entries_),
        std::end(this->binary_->dynamic_entries_),
        [] (const DynamicEntry* entry)
        {
          return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_INIT_ARRAYSZ;
        });

    DynamicEntry* dt_initarray_entry = *it_dt_initarray;

    if (it_dt_initarray_size != std::end(this->binary_->dynamic_entries_)) {
      std::vector<uint64_t>& array = dt_initarray_entry->array();

      const uint32_t nb_functions = static_cast<uint32_t>((*it_dt_initarray_size)->value() / sizeof(uint__));
      try {
        const uint64_t offset = this->binary_->virtual_address_to_offset(dt_initarray_entry->value());
        const uint__* rawArray = reinterpret_cast<const uint__*>(
            this->stream_->read(offset, nb_functions * sizeof(uint__)));

        for (size_t i = 0; i < nb_functions; ++i) {
          array.push_back(rawArray[i]);
        }
      } catch (const std::exception&) {
        LOG(WARNING) << "Unable to fetch init array";
      }
    } else {
      // TODO: Has DT_INIT but not DT_INIT_SIZE
    }
  }


  // Check for FINI array
  // ====================
  auto&& it_dt_finiarray = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry)
      {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_FINI_ARRAY;
      });

  if (it_dt_finiarray != std::end(this->binary_->dynamic_entries_)) {

    auto&& it_dt_finiarray_size = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry)
      {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_FINI_ARRAYSZ;
      });

    if (it_dt_finiarray_size != std::end(this->binary_->dynamic_entries_)) {

      DynamicEntry* dt_finiarray_entry = *it_dt_finiarray;

      std::vector<uint64_t>& array = dt_finiarray_entry->array();
      const uint32_t nb_functions = static_cast<uint32_t>((*it_dt_finiarray_size)->value() / sizeof(uint__));
      try {
        const uint64_t offset = this->binary_->virtual_address_to_offset(dt_finiarray_entry->value());
        const uint__ *rawArray = reinterpret_cast<const uint__*>(
            this->stream_->read(offset, nb_functions * sizeof(uint__)));

        for (size_t i = 0; i < nb_functions; ++i) {
          array.push_back(rawArray[i]);
        }
      } catch (const LIEF::exception&) {
        LOG(WARNING) << "Unable to fetch fini array";
      }
    } else {
      //TOSO
    }
  }


  // Check for PREINIT array
  // =======================
  auto&& it_dt_preinitarray = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry)
      {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_PREINIT_ARRAY;
      });

  if (it_dt_preinitarray != std::end(this->binary_->dynamic_entries_)) {

    auto&& it_dt_preinitarray_size = std::find_if(
      std::begin(this->binary_->dynamic_entries_),
      std::end(this->binary_->dynamic_entries_),
      [] (const DynamicEntry* entry)
      {
        return entry != nullptr and entry->tag() == DYNAMIC_TAGS::DT_PREINIT_ARRAYSZ;
      });

    if (it_dt_preinitarray_size != std::end(this->binary_->dynamic_entries_)) {

      DynamicEntry* dt_preinitarray_entry = *it_dt_preinitarray;

      std::vector<uint64_t>& array = dt_preinitarray_entry->array();
      const uint32_t nb_functions = static_cast<uint32_t>((*it_dt_preinitarray_size)->value() / sizeof(uint__));
      try {
        const uint64_t offset = this->binary_->virtual_address_to_offset(dt_preinitarray_entry->value());
        const uint__ *rawArray = reinterpret_cast<const uint__*>(
            this->stream_->read(offset, nb_functions * sizeof(uint__)));

        for (size_t i = 0; i < nb_functions; ++i) {
          array.push_back(rawArray[i]);
        }
      } catch (const LIEF::exception&) {
        LOG(WARNING) << "Unable to fetch preinit array";
      }
    } else {
      //TODO: has DT_FINI but not DT_FINISZ
    }
  }
}


template<typename ELF_T>
void Parser::parse_pltgot_relocations(uint64_t offset, uint64_t size, bool isRela) {
  using Elf_Rela = typename ELF_T::Elf_Rela;
  using Elf_Rel  = typename ELF_T::Elf_Rel;

  // Already Parsed
  if (this->binary_->pltgot_relocations_.size() > 0) {
    return;
  }

  const uint64_t offset_relocations = offset;
  const uint8_t shift = std::is_same<ELF_T, ELF32>::value ? 8 : 32;

  if (isRela) {
    const uint32_t nb_entries = static_cast<uint32_t>(size / sizeof(Elf_Rela));
    const Elf_Rela* relocEntry = reinterpret_cast<const Elf_Rela*>(
        this->stream_->read(offset_relocations, nb_entries * sizeof(Elf_Rela)));

    for (uint32_t i = 0; i < nb_entries; ++i) {
      Relocation* reloc = new Relocation{relocEntry};
      reloc->architecture_ = this->binary_->header_.machine_type();

      const uint32_t idx  = static_cast<uint32_t>(relocEntry->r_info >> shift);
      if (idx > 0 and idx < this->binary_->dynamic_symbols_.size()) {
        reloc->symbol_ = this->binary_->dynamic_symbols_[idx];
      }

      this->binary_->pltgot_relocations_.push_back(reloc);
      relocEntry++;
    }
  } else {
    const uint32_t nb_entries = static_cast<uint32_t>(size / sizeof(Elf_Rel));
    const Elf_Rel* relocEntry = reinterpret_cast<const Elf_Rel*>(
        this->stream_->read(offset_relocations, nb_entries * sizeof(Elf_Rel)));
    for (uint32_t i = 0; i < nb_entries; ++i) {
      Relocation* reloc = new Relocation{relocEntry};
      reloc->architecture_ = this->binary_->header_.machine_type();

      const uint32_t idx =  static_cast<uint32_t>(relocEntry->r_info >> shift);
      if (idx > 0 and idx < this->binary_->dynamic_symbols_.size()) {
        reloc->symbol_ = this->binary_->dynamic_symbols_[idx];
      }

      this->binary_->pltgot_relocations_.push_back(reloc);
      relocEntry++;
    }
  }
}


template<typename ELF_T>
void Parser::parse_symbol_version_requirement(uint64_t offset, uint32_t nb_entries) {
  using Elf_Verneed = typename ELF_T::Elf_Verneed;
  using Elf_Vernaux = typename ELF_T::Elf_Vernaux;

  LOG(DEBUG) << "[+] Build Symbol version requirement";

  const uint64_t svr_offset = offset;

  LOG(DEBUG) << "Symbol version requirement offset: 0x" << std::hex << svr_offset;

  const uint64_t string_offset = this->get_dynamic_string_table();

  if (string_offset == 0) {
    LOG(WARNING) << "Unable to find the .dynstr section";
  }

  uint32_t next_symbol_offset = 0;

  for (uint32_t symbolCnt = 0; symbolCnt < nb_entries; ++symbolCnt) {

    const Elf_Verneed* header = reinterpret_cast<const Elf_Verneed*>(
        this->stream_->read(
          svr_offset + next_symbol_offset,
          sizeof(Elf_Verneed)));

    SymbolVersionRequirement* symbol_version_requirement= new SymbolVersionRequirement{header};
    if (string_offset != 0) {
      symbol_version_requirement->name({
          this->stream_->read_string(string_offset + header->vn_file)});
    }

    const uint32_t nb_symbol_aux = header->vn_cnt;

    uint32_t next_aux_offset = 0;
    if (nb_symbol_aux > 0 and header->vn_aux > 0) {

      const Elf_Vernaux* aux_header = reinterpret_cast<const Elf_Vernaux*>(
          this->stream_->read(
            svr_offset + next_symbol_offset + header->vn_aux,
            sizeof(Elf_Vernaux)));

      for (uint32_t j = 0; j < nb_symbol_aux; ++j) {
        aux_header = reinterpret_cast<const Elf_Vernaux*>(
            this->stream_->read(
              svr_offset + next_symbol_offset + header->vn_aux + next_aux_offset,
              sizeof(Elf_Vernaux)));


        SymbolVersionAuxRequirement* svar = new SymbolVersionAuxRequirement{aux_header};
        if (string_offset != 0) {
          svar->name({this->stream_->read_string(string_offset + aux_header->vna_name)});
        }

        symbol_version_requirement->symbol_version_aux_requirement_.push_back(svar);
        if (aux_header->vna_next == 0) break;
        next_aux_offset += aux_header->vna_next;
      }

      this->binary_->symbol_version_requirements_.push_back(symbol_version_requirement);
    }

    if (header->vn_next == 0) break;
    next_symbol_offset += header->vn_next;

  }


  // Associate Symbol Version with auxiliary symbol
  // Symbol version requirement is used to map
  // SymbolVersion::SymbolVersionAux <------> SymbolVersionAuxRequirement
  //
  // We mask the 15th (7FFF) bit because it sets if this symbol is a hidden on or not
  // but we don't care
  for (SymbolVersionRequirement* svr : this->binary_->symbol_version_requirements_) {
    for (SymbolVersionAuxRequirement* svar : svr->symbol_version_aux_requirement_) {
      std::for_each(
          std::begin(this->binary_->symbol_version_table_),
          std::end(this->binary_->symbol_version_table_),
          [&svar] (SymbolVersion* sv)
          {
            if ((sv->value() & 0x7FFF) == svar->other()) {
              sv->symbol_aux_ = svar;
            }
          });
    }
  }
}


template<typename ELF_T>
void Parser::parse_symbol_version_definition(uint64_t offset, uint32_t nb_entries) {
  using Elf_Verdef = typename ELF_T::Elf_Verdef;
  using Elf_Verdaux = typename ELF_T::Elf_Verdaux;

  const uint64_t string_offset = this->get_dynamic_string_table();
  uint32_t next_symbol_offset = 0;

  for (uint32_t i = 0; i < nb_entries; ++i) {
    const Elf_Verdef* svd_header = reinterpret_cast<const Elf_Verdef*>(
        this->stream_->read(
        offset + next_symbol_offset,
        sizeof(Elf_Verdef)));

    SymbolVersionDefinition* symbol_version_definition = new SymbolVersionDefinition{svd_header};
    uint32_t nb_aux_symbols = svd_header->vd_cnt;
    uint32_t next_aux_offset = 0;
    for (uint32_t j = 0; j < nb_aux_symbols; ++j) {
      const Elf_Verdaux* svda_header = reinterpret_cast<const Elf_Verdaux*>(
          this->stream_->read(
          offset + next_symbol_offset + svd_header->vd_aux + next_aux_offset,
          sizeof(Elf_Verdaux)));
      if (string_offset != 0) {
        std::string name  = {this->stream_->read_string(string_offset + svda_header->vda_name)};
        symbol_version_definition->symbol_version_aux_.push_back(new SymbolVersionAux{name});
      }

      // Additional check
      if (svda_header->vda_next == 0) break;

      next_aux_offset += svda_header->vda_next;
    }

    this->binary_->symbol_version_definition_.push_back(symbol_version_definition);

    // Additional check
    if (svd_header->vd_next == 0) break;

    next_symbol_offset += svd_header->vd_next;
  }

  // Associate Symbol Version with auxiliary symbol
  // We mask the 15th bit because it sets if this symbol is a hidden on or not
  // but we don't care
  for (SymbolVersionDefinition& svd : this->binary_->get_symbols_version_definition()) {
    for (SymbolVersionAux* sva : svd.symbol_version_aux_) {
      std::for_each(
          std::begin(this->binary_->symbol_version_table_),
          std::end(this->binary_->symbol_version_table_),
          [&sva, &svd] (SymbolVersion* sv)
          {
            if (svd.ndx() > 1 and (sv->value() & 0x7FFF) == svd.ndx() ) {
              sv->symbol_aux_ = sva;
            }

          });
    }
  }
}

// See: https://github.com/lattera/glibc/blob/master/elf/dl-lookup.c#L860
// and  https://github.com/lattera/glibc/blob/master/elf/dl-lookup.c#L226
template<typename ELF_T>
void Parser::parse_symbol_gnu_hash(uint64_t offset) {
  using uint__  = typename ELF_T::uint;

  LOG(DEBUG) << "[+] Build symbol GNU hash";
  GnuHash gnuhash;

  uint64_t current_offset = offset;

  const uint32_t* header = reinterpret_cast<const uint32_t*>(
      this->stream_->read(current_offset, 4 * sizeof(uint32_t)));

  current_offset += 4 * sizeof(uint32_t);

  const uint32_t nbuckets  = header[0];
  const uint32_t symndx    = header[1];
  const uint32_t maskwords = header[2];
  const uint32_t shift2    = header[3];

  gnuhash.symbol_index_ = symndx;
  gnuhash.shift2_       = shift2;

  if (maskwords & (maskwords - 1)) {
    LOG(WARNING) << "maskwords is not a power of 2";
  }

  //LOG(DEBUG) << "nbuckets: %d",nbuckets;
  //LOG(DEBUG) << "symndx: %d",  symndx;
  //LOG(DEBUG) << "maskwords: %" PRIx32 "", maskwords;
  //LOG(DEBUG) << "shift2: %" PRIx32 "",    shift2;

  try {
    std::vector<uint64_t> bloom_filters(maskwords);

    for (size_t i = 0; i < maskwords; ++i) {
      bloom_filters[i] = this->stream_->read_integer<uint__>(current_offset);
      current_offset += sizeof(uint__);
    }

    gnuhash.bloom_filters_ = std::move(bloom_filters);
  }
  catch (const read_out_of_bound&) {
    throw corrupted("GNU Hash, maskwords corrupted");
  }
  catch (const std::bad_alloc&) {
    throw corrupted("GNU Hash, maskwords corrupted");
  }

  std::vector<uint32_t> buckets;
  buckets.reserve(std::min<uint32_t>(nbuckets, 400));
  try {
    const uint32_t* hash_buckets = reinterpret_cast<const uint32_t*>(
        this->stream_->read(current_offset, nbuckets * sizeof(uint32_t)));
    current_offset += nbuckets * sizeof(uint32_t);

    buckets = {hash_buckets, hash_buckets + nbuckets};
  } catch (const read_out_of_bound&) {
    throw corrupted("GNU Hash, hash_buckets corrupted");
  }

  gnuhash.buckets_ = std::move(buckets);

  const uint32_t dynsymcount = static_cast<uint32_t>(this->binary_->dynamic_symbols_.size());
  //LOG(DEBUG) << "dynsymcount: %" PRId32 "", dynsymcount;
  if (dynsymcount <= symndx) {
    throw corrupted("GNU Hash, symndx corrupted");
  }

  uint32_t nb_hash = dynsymcount - symndx;

  std::vector<uint32_t> hashvalues;
  hashvalues.reserve(std::min<uint32_t>(nb_hash, 400));

  try {
    const uint32_t* hash_values = reinterpret_cast<const uint32_t*>(
        this->stream_->read(current_offset, nb_hash * sizeof(uint32_t)));

    hashvalues = {hash_values, hash_values + nb_hash};
  } catch (const read_out_of_bound&) {
    throw corrupted("GNU Hash, nb_hash corrupted");
  }

  gnuhash.hash_values_ = std::move(hashvalues);
  this->binary_->gnu_hash_ = std::move(gnuhash);

}


}
}
