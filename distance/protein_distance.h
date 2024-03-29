//
// Created by krab1k on 21.01.20.
//

#pragma once

#include <string>
#include <memory>

#include "gesamtlib/gsmt_aligner.h"
#undef DefineClass // JNI conflicts with GESAMT macro, so undef it

enum status {
    RESULT_OK,
    RESULT_DISSIMILAR,
};

std::shared_ptr<gsmt::Structure> load_single_structure(const std::string &id, const std::string &directory, bool binary);


void init_library(const std::string &archive_directory, int cache_size);

void close_library();

enum status run_computation(const std::string &id1, const std::string &id2, float min_qscore, std::unique_ptr<gsmt::Superposition> &SD);

std::shared_ptr<gsmt::Structure> get_structure(const std::string &id);
