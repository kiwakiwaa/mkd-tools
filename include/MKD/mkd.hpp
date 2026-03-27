//
// kiwakiwaaにより 2026/02/20 に作成されました。
//

#pragma once

#include "MKD/result.hpp"

#include "MKD/dictionary/dictionary_product.hpp"
#include "MKD/dictionary/dictionary.hpp"
#include "MKD/dictionary/metadata.hpp"
#include "MKD/dictionary/paths.hpp"

#include "MKD/output/headline_exporter.hpp"
#include "MKD/output/keystore_exporter.hpp"
#include "MKD/output/resource_exporter.hpp"

#include "MKD/platform/directory_dictionary_source.hpp"

#ifdef __APPLE__
#include "MKD/platform/macos/macos_dictionary_source.hpp"
#include "MKD/platform/macos/monokakido_receipt.hpp"
#endif

#include "MKD/resource/resource_loader.hpp"
#include "MKD/resource/keystore.hpp"
#include "MKD/resource/headline_store.hpp"
#include "MKD/resource/nrsc.hpp"
#include "MKD/resource/rsc.hpp"
#include "MKD/resource/font.hpp"
#include "MKD/resource/retained_span.hpp"
#include "MKD/resource/xml_view.hpp"
