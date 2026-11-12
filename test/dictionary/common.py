from pathlib import Path

import mkd


ROOT = Path(__file__).parent
ASSETS = ROOT / 'assets'


def new_product(destination, profile=mkd.CreationProfile.GENERAL):
    return mkd.ProductWriter.create(
        destination,
        mkd.DictionaryMetadata(
            display_name=mkd.LocalizedString(en='Birbs & Saberlion', ja='小鳥とセイバーライオン'),
            version='1.0',
        ),
        profile=profile,
    )


def new_dictionary(product, identifier='test.birbs', directory='birbs', *, encrypted=False, dictionary_id=1000):
    return product.add_dictionary(
        mkd.DictionaryContent(
            identifier=identifier,
            directory=directory,
            dictionary_id=dictionary_id,
            title=mkd.LocalizedString(ja='小鳥たちの辞典'),
            languages=['ja', 'en'],
        ),
        encrypt_contents=encrypted,
    )


def add_headword(dictionary, target, title, reading, *aliases):
    dictionary.add_headline(mkd.HeadlineComponents(title, target, sorting_headline=reading))
    for key in dict.fromkeys((title, reading, *aliases)):
        dictionary.add_search_key(mkd.SearchScope.HEADWORD, key, target)


def write_article(entry, filename, **values):
    template = (ROOT / 'articles' / filename).read_text(encoding='utf-8')
    entry.write_xml(template.format(**values))


def one_birb(dictionary, *, page_id=None):
    entry = dictionary.create_entry(page_id=page_id)
    entry.write_xml('<html><body><h1>雀</h1><p>A small birb.</p></body></html>')
    add_headword(dictionary, entry.target, '雀', 'すずめ')
    return entry.target
