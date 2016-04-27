/*

    ParaText: parallel text reading
    Copyright (C) 2016. wise.io, Inc.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
  Coder: Damian Eads.
 */

#ifndef PARATEXT_ROW_BASED_LOADER_HPP
#define PARATEXT_ROW_BASED_LOADER_HPP

#include "parse_params.hpp"
#include "rowbased_worker.hpp"
#include "chunker.hpp"
#include "header_parser.hpp"

namespace ParaText {

namespace CSV {

class RowBasedLoader {
public:
  RowBasedLoader() : length_(0) {}

  void load(const std::string &filename, const ParseParams &params) {
      header_parser_.open(filename, params.no_header);
      struct stat fs;
      if (stat(filename.c_str(), &fs) == -1) {
        throw std::logic_error("cannot stat file");
      }
      length_ = fs.st_size;
      column_infos_.resize(header_parser_.get_num_columns());
      for (size_t i = 0; i < column_infos_.size(); i++) {
        column_infos_[i].name = header_parser_.get_column_name(i);
      }
      if (header_parser_.has_header()) {
        chunker_.process(filename, header_parser_.get_end_of_header()+1, params.num_threads, params.allow_quoted_newlines);
      }
      else {
        chunker_.process(filename, 0, params.num_threads, params.allow_quoted_newlines);
      }
    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<RowBasedParseWorker> > workers;
    for (size_t worker_id = 0; worker_id < params.num_threads; worker_id++) {
      size_t start_of_chunk, end_of_chunk = 0;
      std::tie(start_of_chunk, end_of_chunk) = chunker_.get_chunk(worker_id);
      
      /* If the chunk was eliminated because its entirety represents quoted
         text, do not spawn a worker thread for it. */
      if (start_of_chunk == end_of_chunk) {
        continue;
      }
      workers.push_back(std::make_shared<RowBasedParseWorker>(start_of_chunk, end_of_chunk, length_, params.block_size, params.compression == Compression::SNAPPY));
      threads.emplace_back(&RowBasedParseWorker::parse,
                           workers.back(),
                           filename);
      start_of_chunk = end_of_chunk;
    }
    for (size_t i = 0; i < threads.size(); i++) {
      threads[i].join();
    }
  }

    /*
      Returns the number of columns parsed by this loader.
     */
    size_t     get_num_columns() const {
      return column_infos_.size();
    }

    /*
      Returns the info about the column.
     */
    ParaText::ColumnInfo get_column_info(size_t column_index) const {
      return column_infos_[column_index];
    }

    /*
      Returns the categorical levels.
     */
    const std::vector<std::string> &get_levels(size_t column_index) const {
      std::cout << level_names_[column_index].size();
      return level_names_[column_index];
    }

    size_t size() const {
      return size_.back();
    }

private:
  size_t length_;
  mutable std::vector<std::unordered_map<std::string, size_t> > level_ids_;
  mutable std::vector<std::vector<std::string> > level_names_;
  std::vector<size_t> size_;
  std::vector<ColumnInfo> column_infos_;
  TextChunker chunker_;
  HeaderParser header_parser_;
};
}
}
#endif