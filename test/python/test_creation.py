import array
import gc
import xml.etree.ElementTree as ET

import mkd
from support import DictionaryTest, new_dictionary, new_product, one_birb


class CreationTests(DictionaryTest):
    def test_preflight_is_bounded_deterministic_and_keeps_distinct_source_origins(self):
        reports = []
        for pages in ((1, 2, 3, 4), (4, 3, 2, 1)):
            product = new_product(self.workspace() / 'Origins')
            dictionary = new_dictionary(product)
            for page in pages:
                source = mkd.SourceContext(filename='birds.xml', record=str(page))
                entry = dictionary.create_entry(page_id=page, source=source)
                entry.write_xml('<html>\n<body><img src="missing.png"/><img src="missing.png"/></body></html>')
                dictionary.add_headline(mkd.HeadlineComponents('鳥', entry.target))
                dictionary.add_search_key(mkd.SearchScope.HEADWORD, 'bird', entry.target)
            report = dictionary.preflight()
            self.assertIsInstance(report.issues, tuple)
            issue, = [i for i in report.issues if i.severity == mkd.IssueSeverity.ERROR]
            self.assertEqual(issue.code, 'missing-resource')
            self.assertEqual(issue.occurrences, 8)
            self.assertIsInstance(issue.origins, tuple)
            self.assertEqual(len(issue.origins), 3)
            self.assertEqual(issue.origins[0].context.filename, 'birds.xml')
            self.assertEqual(issue.origins[0].location.line, 2)
            with self.assertRaises(AttributeError):
                issue.code = 'changed'
            reports.append(report.describe())
            limited = dictionary.preflight(options=mkd.ValidationOptions(max_issues=0))
            self.assertEqual(limited.issues, ())
            self.assertFalse(limited.valid)
            self.assertTrue(limited.truncated)
            with self.assertRaises(mkd.Error):
                dictionary.finish()
            dictionary.add_asset('missing.png', b'')
            dictionary.finish()
            product.finish()
            self.assertFalse(report.valid)
        self.assertEqual(*reports)

    def test_product_preflight_allows_asset_and_menu_repairs_before_sealing(self):
        for profile in (mkd.CreationProfile.GENERAL, mkd.CreationProfile.KANWA):
            with self.subTest(profile=profile):
                root = self.workspace()
                path = root / 'Repair'
                product = new_product(path, profile)
                dictionary = new_dictionary(product)
                target = one_birb(dictionary)
                source = mkd.SourceContext(filename='menus.json', record='manuals')
                product.set_appendices(mkd.AppendixMenu([
                    mkd.AppendixSection('', [
                        mkd.AppendixItem('Manual', mkd.AppendixDocument('birbs/manual.html')),
                        mkd.AppendixItem('About', mkd.AppendixDocument('about.html')),
                    ])
                ]), source=source)
                product.set_information(mkd.ProductInformation(licenses=[
                    mkd.ProductLicense('Terms', page='legal.html'),
                ]))
                product.add_asset('corrupt.entries', b'not an entry list')
                product.set_navigation(mkd.NavigationIndex(sections=[mkd.NavigationSection('Manuals', [
                    mkd.NavigationItem(label='Missing', target=mkd.AppendixEntryLink(mkd.EntryId(99), 1000)),
                    mkd.NavigationItem(label='Manual', target=mkd.AppendixDocument('first.html')),
                    mkd.NavigationItem(label='Manual', target=mkd.AppendixDocument('second.html')),
                    mkd.NavigationItem(label='Missing list', target=mkd.AppendixList('missing.entries')),
                    mkd.NavigationItem(label='Corrupt list', target=mkd.AppendixList('corrupt.entries')),
                    mkd.NavigationItem(label='Outside list', target=mkd.AppendixList('../outside.entries')),
                ])]), source=source)

                def snapshot():
                    return {p.relative_to(root): p.read_bytes() if p.is_file() else None for p in root.rglob('*')}

                before = snapshot()
                report = product.preflight()
                self.assertGreaterEqual(report.total_issues, 8)
                self.assertIsInstance(report.pending_dictionaries, tuple)
                self.assertEqual(report.pending_dictionaries, ('test.birbs',))
                self.assertEqual(report.describe(), product.preflight().describe())
                for name, position in (('first.html', 'item 2 Manual'), ('second.html', 'item 3 Manual')):
                    issue, = [i for i in report.issues if name in i.message]
                    self.assertIn(position, issue.origins[0].subject)
                    self.assertEqual(issue.origins[0].context.filename, 'menus.json')
                    self.assertEqual(issue.origins[0].context.record, 'manuals')
                for name in ('missing.entries', 'corrupt.entries', '../outside.entries'):
                    self.assertTrue(any(name in i.message for i in report.issues), name)
                self.assertFalse(any('about.html' in i.message for i in report.issues))
                with self.assertRaises(mkd.Error):
                    product.finish()
                self.assertEqual(before, snapshot())
                dictionary.add_asset('manual.html', b'<html/>')
                product.add_asset('legal.html', b'<html/>')
                entries = dictionary.add_appendix_entries('birds', 'Birds', [mkd.AppendixEntry(target)])
                product.set_navigation(mkd.NavigationIndex(sections=[mkd.NavigationSection('', [
                    mkd.NavigationItem(label='Bird', target=mkd.AppendixEntryLink(target, 1000)),
                    mkd.NavigationItem(label='Birds', target=entries),
                ])]))
                self.assertTrue(product.preflight().valid)
                self.assertFalse(product.preflight().ready)
                dictionary.finish()
                self.assertTrue(product.preflight().ready)
                product.finish()
                self.assertTrue((path / 'Contents/about.html').is_file())

    def test_kanwa_passage_keys_search_their_normalized_phrases(self):
        path = self.workspace() / 'Passages'
        product = new_product(path, mkd.CreationProfile.KANWA)
        dictionary = new_dictionary(product)
        entry = dictionary.create_entry()
        sense = entry.add_target(mkd.EntryTargetKind.SENSE)
        example = entry.add_target(mkd.EntryTargetKind.EXAMPLE)
        dictionary.add_headline(mkd.HeadlineComponents('鳥', entry.target))
        dictionary.add_headline(mkd.HeadlineComponents('鳥の意味', sense))
        dictionary.add_headline(mkd.HeadlineComponents('鳥の用例', example))
        dictionary.add_search_key(mkd.SearchScope.HEADWORD, '鳥', entry.target)
        dictionary.add_search_key(mkd.SearchScope.SENSE, '鳥などが声を出す', sense)
        entry.write_xml(f'<html><body><p id="{sense}">意味</p><p id="{example}">用例</p></body></html>')
        for term in ('鳥などが声を出す', '鳴禽', '鳴禽'):
            dictionary.add_search_key(mkd.SearchScope.SENSE, term, sense)
        dictionary.add_search_key(mkd.SearchScope.EXAMPLE, '春の林で鳴禽の声を聞く。', example)
        passages = dictionary.add_appendix_entries('passages', 'Passages', [
            mkd.AppendixEntry(sense), mkd.AppendixEntry(example, label='Example'),
        ])
        product.set_appendices(mkd.AppendixMenu([mkd.AppendixSection('', [
            mkd.AppendixItem('Page headlines', mkd.AppendixList(passages.path)),
            mkd.AppendixItem('Anchor headlines', mkd.AppendixList(passages.path, using_anchor_id=True)),
        ])]))
        product.set_navigation(mkd.NavigationIndex())
        dictionary.finish()
        product.finish()
        opened = mkd.DictionaryProduct.open(path)
        appendices = opened.navigation.item_at(0, 0).target
        for i in range(2):
            entries = appendices.item_at(0, i).target
            self.assertEqual([entries.item_at(0, row).target.entry_id for row in range(2)], [sense, example])
        reopened = opened.dictionaries[0]
        search = mkd.DictionarySearch(reopened)
        for scope, term, target in (
            (mkd.SearchScope.SENSE, '鳥などが声を出す', sense),
            (mkd.SearchScope.SENSE, '鳴禽', sense),
            (mkd.SearchScope.EXAMPLE, '春の林で鳴禽の声を聞く。', example),
        ):
            for policy in (mkd.SearchFallback.NONE, mkd.SearchFallback.DICTIONARY_RULES):
                self.assertEqual(list(search.search(term, scope=scope, fallback=policy,
                                                   match=mkd.SearchMode.EXACT)), [target])
        for term in ('声を', '声ヲ', '声 鳥', '声／鳥'):
            self.assertEqual(list(search.search(term, scope=mkd.SearchScope.SENSE)), [sense])
        for term in ('鳥声', '出す鳴禽', '聞く春'):
            self.assertEqual(list(search.search(term, scope=mkd.SearchScope.SENSE)), [])
        self.assertEqual(list(search.search('鳥', scope=mkd.SearchScope.SENSE,
                                            fallback=mkd.SearchFallback.NONE,
                                            match=mkd.SearchMode.EXACT)), [sense])
        xml = ET.fromstring(reopened.entry_xml(entry.target.page_id))
        for target in (sense, example):
            companion = mkd.EntryId(target.page_id, target.item_id + 0x800)
            self.assertIsNone(xml.find(f'.//*[@id="{companion}"]'))

    def test_exact_only_passage_keys_retain_raw_exact_lookup(self):
        for profile in (mkd.CreationProfile.GENERAL, mkd.CreationProfile.KANWA):
            path = self.workspace() / 'Exact'
            product = new_product(path, profile)
            dictionary = new_dictionary(product)
            target = one_birb(dictionary)
            for scope in (mkd.SearchScope.SENSE, mkd.SearchScope.EXAMPLE):
                for term in ('鳥', '鳴禽'):
                    dictionary.add_search_key(scope, term, target, exact_only=True)
            dictionary.finish()
            product.finish()
            reopened = mkd.DictionaryProduct.open(path).dictionaries[0]
            search = mkd.DictionarySearch(reopened)
            for scope in (mkd.SearchScope.SENSE, mkd.SearchScope.EXAMPLE):
                for term in ('鳥', '鳴禽'):
                    self.assertEqual(list(search.search(term, scope=scope, match=mkd.SearchMode.EXACT,
                                                       fallback=mkd.SearchFallback.NONE)), [target])
                    self.assertEqual(list(search.search(term, scope=scope,
                                                       fallback=mkd.SearchFallback.NONE)), [])
                self.assertEqual(list(search.search('鳴', scope=scope)), [])

    def test_passage_components_require_consistent_key_options(self):
        for exact_first in (True, False):
            product = new_product(self.workspace() / 'Options', mkd.CreationProfile.KANWA)
            dictionary = new_dictionary(product)
            target = dictionary.create_entry().target
            calls = [('声', True), ('鳥声', False)]
            if not exact_first:
                calls.reverse()
            dictionary.add_search_key(mkd.SearchScope.SENSE, calls[0][0], target, exact_only=calls[0][1])
            with self.assertRaisesRegex(mkd.Error, "Search key.*sense.*Conflicting options"):
                dictionary.add_search_key(mkd.SearchScope.SENSE, calls[1][0], target, exact_only=calls[1][1])

    def test_sense_and_example_indexes_have_distinct_targets(self):
        for scopes in ((mkd.SearchScope.SENSE, mkd.SearchScope.EXAMPLE),
                       (mkd.SearchScope.EXAMPLE, mkd.SearchScope.SENSE)):
            product = new_product(self.workspace() / 'Scopes', mkd.CreationProfile.KANWA)
            dictionary = new_dictionary(product)
            target = dictionary.create_entry().target
            dictionary.add_search_key(scopes[0], '鳥声', target)
            with self.assertRaisesRegex(mkd.Error, 'distinct targets'):
                dictionary.add_search_key(scopes[1], '声鳥', target)

    def test_kanwa_passage_companions_cannot_replace_real_anchors(self):
        for keys_first in (True, False):
            product = new_product(self.workspace() / 'Collision', mkd.CreationProfile.KANWA)
            dictionary = new_dictionary(product)
            entry = dictionary.create_entry()
            target = entry.add_target(mkd.EntryTargetKind.SENSE)
            companion = mkd.EntryId(target.page_id, target.item_id + 0x800)
            dictionary.add_headline(mkd.HeadlineComponents('鳥', entry.target))
            dictionary.add_headline(mkd.HeadlineComponents('鳥声', target))
            dictionary.add_search_key(mkd.SearchScope.HEADWORD, '鳥', entry.target)
            if keys_first:
                dictionary.add_search_key(mkd.SearchScope.SENSE, '鳥声', target)
            entry.write_xml(f'<html><body><p id="{target}">鳥声</p><span id="{companion}"/></body></html>')
            if not keys_first:
                dictionary.add_search_key(mkd.SearchScope.SENSE, '鳥声', target)
            report = dictionary.preflight()
            self.assertIn('passage-companion-collision', [issue.code for issue in report.issues])
            with self.assertRaisesRegex(mkd.Error, 'reserves ID.*automatically generated search record'):
                dictionary.finish()

    def test_passage_search_record_ids_must_fit(self):
        product = new_product(self.workspace() / 'Overflow', mkd.CreationProfile.KANWA)
        dictionary = new_dictionary(product)
        with self.assertRaisesRegex(mkd.Error, 'item ID is too high'):
            dictionary.add_search_key(mkd.SearchScope.SENSE, '鳥声', mkd.EntryId(1, 0xF800))

    def test_last_explicit_passage_target_has_a_searchable_companion(self):
        path = self.workspace() / 'LastPassage'
        product = new_product(path, mkd.CreationProfile.KANWA)
        dictionary = new_dictionary(product)
        target = mkd.EntryId(1, 0xF7FF)
        dictionary.add_page(1, f'<html><body><p id="{target}">鳥声</p></body></html>')
        dictionary.add_headline(mkd.HeadlineComponents('鳥', mkd.EntryId(1)))
        dictionary.add_headline(mkd.HeadlineComponents('鳥声', target))
        dictionary.add_search_key(mkd.SearchScope.HEADWORD, '鳥', mkd.EntryId(1))
        dictionary.add_search_key(mkd.SearchScope.SENSE, '鳥声', target)
        dictionary.finish()
        product.finish()
        reopened = mkd.DictionaryProduct.open(path).dictionaries[0]
        self.assertEqual(list(mkd.DictionarySearch(reopened).search('鳥声', scope=mkd.SearchScope.SENSE)), [target])

    def test_kanwa_target_capacity_reserves_companion_space(self):
        for kind, last in ((mkd.EntryTargetKind.SENSE, 0xF7FF), (mkd.EntryTargetKind.EXAMPLE, 0x57FF)):
            product = new_product(self.workspace() / 'Capacity', mkd.CreationProfile.KANWA)
            entry = new_dictionary(product).create_entry()
            for _ in range(2047):
                target = entry.add_target(kind)
            self.assertEqual(target.item_id, last)
            with self.assertRaisesRegex(mkd.Error, 'range exhausted'):
                entry.add_target(kind)

    def test_minimal_authoring_publishes_a_searchable_dictionary(self):
        path = self.workspace() / 'Birbs'
        product = new_product(path)
        dictionary = new_dictionary(product, encrypted=True)
        target = one_birb(dictionary)
        dictionary.finish()
        self.assertFalse(path.exists())
        product.finish()
        reopened = mkd.DictionaryProduct.open(path).dictionaries[0]
        self.assertEqual(list(mkd.DictionarySearch(reopened).search('すずめ')), [target])
        self.assertEqual(list(reopened.entry_order), [target.page_id])
        self.assertIsNone(reopened.entry_order.next(target.page_id))

    def test_navigation_round_trip_keeps_subentry_targets(self):
        path = self.workspace() / 'Birbs'
        product = new_product(path)
        dictionary = new_dictionary(product)
        entry = dictionary.create_entry()
        compound = entry.add_target(mkd.EntryTargetKind.COMPOUND)
        entry.write_xml(f'<html><body><p id="{compound}">雀躍</p></body></html>')
        dictionary.add_headline(mkd.HeadlineComponents('雀', entry.target))
        dictionary.add_headline(mkd.HeadlineComponents('雀躍', compound))
        dictionary.add_search_key(mkd.SearchScope.HEADWORD, 'すずめ', entry.target)
        product.set_navigation(mkd.NavigationIndex(sections=[
            mkd.NavigationSection(items=[mkd.NavigationItem(label='雀躍', target=compound)]),
        ]))
        dictionary.finish()
        product.finish()
        reopened = mkd.DictionaryProduct.open(path)
        navigation = reopened.navigation
        del reopened
        gc.collect()
        item = navigation.item_at(0, 0)
        self.assertEqual(item.label, '雀躍')
        self.assertEqual(item.target.entry_id, compound)

    def test_entry_handles_keep_the_writer_alive(self):
        path = self.workspace() / 'Birbs'
        product = new_product(path)
        dictionary = new_dictionary(product)
        entry = dictionary.create_entry()
        del product, dictionary
        gc.collect()
        self.assertEqual(entry.target, mkd.EntryId(1))
        entry.write_xml('<html><body>A birb waiting to hatch.</body></html>')
        del entry
        gc.collect()
        self.assertFalse(path.exists())

    def test_allocated_targets_connect_xml_headlines_and_search_keys(self):
        path = self.workspace() / 'Birbs'
        product = new_product(path)
        dictionary = new_dictionary(product)
        entry = dictionary.create_entry()
        idiom = entry.add_target(mkd.EntryTargetKind.IDIOM)
        entry.write_xml(f'<html><body><p id="{idiom}">一石二鳥</p></body></html>')
        dictionary.add_headline(mkd.HeadlineComponents('鳥', entry.target))
        dictionary.add_headline(mkd.HeadlineComponents('一石二鳥', idiom))
        dictionary.add_search_key(mkd.SearchScope.HEADWORD, 'とり', entry.target)
        dictionary.add_search_key(mkd.SearchScope.IDIOM, 'いっせきにちょう', idiom)
        dictionary.finish()
        product.finish()
        reopened = mkd.DictionaryProduct.open(path).dictionaries[0]
        self.assertEqual(str(idiom), '00001-4001')
        self.assertEqual(list(mkd.DictionarySearch(reopened).search(
            'いっせきにちょう', scope=mkd.SearchScope.IDIOM)), [idiom])
        self.assertIsNotNone(ET.fromstring(reopened.entry_xml(idiom.page_id)).find(f'.//*[@id="{idiom}"]'))

    def test_borrowed_input_bytes_are_consumed_during_the_call(self):
        path = self.workspace() / 'Birbs'
        product = new_product(path)
        dictionary = new_dictionary(product)
        one_birb(dictionary)
        data = bytearray(b'a sleepy birb')
        dictionary.add_graphic('perch.txt', memoryview(data))
        data[:] = b'x' * len(data)
        dictionary.finish()
        product.finish()
        reopened = mkd.DictionaryProduct.open(path).dictionaries[0]
        self.assertEqual(bytes(reopened.graphics.get('perch.txt')), b'a sleepy birb')

    def test_noncontiguous_and_nonbyte_buffers_are_rejected(self):
        product = new_product(self.workspace() / 'Birbs')
        dictionary = new_dictionary(product)
        for data in (memoryview(b'birbs')[::2], array.array('I', [1, 2])):
            with self.assertRaises((TypeError, BufferError)):
                dictionary.add_graphic('perch.txt', data)

    def test_inline_content_encodes_ruby_links_and_xml_escaping(self):
        text = mkd.InlineText()
        text.append(mkd.Ruby('雀', 'すずめ'))
        text.append(' & ')
        text.append(mkd.EntryLink(mkd.EntryId(3), 'セイバーライオン'))
        self.assertEqual(text.plain_text(), '雀 & セイバーライオン')
        self.assertIn('&amp;', text.to_xml())
        self.assertEqual(mkd.InlineText.from_headline(text.to_headline()).to_xml(), text.to_xml())

    def test_cyclic_and_escaping_appendix_menus_are_rejected(self):
        import plistlib
        import shutil
        from support import DICTIONARIES

        for name, contents in (('cycle', 'Appendix.plist'), ('escape', '../outside.html')):
            path = self.workspace() / 'Birbs'
            shutil.copytree(DICTIONARIES / 'Birbs', path)
            menu = [{'children': [{'name': 'Lost birb', 'contents': contents}]}]
            (path / 'Contents/Appendix.plist').write_bytes(plistlib.dumps(menu))
            with self.assertRaises(mkd.Error):
                mkd.DictionaryProduct.load_metadata(path)

    def test_registered_documents_check_nested_resources_and_requested_fragments(self):
        path = self.workspace() / 'Documents'
        product = new_product(path)
        dictionary = new_dictionary(product)
        one_birb(dictionary)
        dictionary.add_asset('manuals/guide.html', b'''<!doctype html><HTML><BODY>
<IMG SRC=../images/bird%20one.png><a href="other.html#missing">Other</a>
<link rel=stylesheet href=styles/main.css><p id=reading>Reading &copy;
</BODY></HTML>''', source=mkd.SourceContext(filename='manual-source.html', record='guide'))
        dictionary.add_asset('manuals/other.html', b'<html><p id=present>Other</html>')
        dictionary.add_asset('manuals/styles/main.css', b'@import "other.css";p{background:url(../../images/css.png)}')
        dictionary.add_asset('manuals/styles/other.css', b'@import "main.css";')
        dictionary.add_asset('images/css.png', b'css image')
        dictionary.add_appendix('manuals/guide.html#absent', 'Guide')
        dictionary.finish()
        before = {p.relative_to(path.parent): p.read_bytes() for p in path.parent.rglob('*') if p.is_file()}
        report = product.preflight()
        self.assertFalse(report.valid)
        self.assertIn('missing-document-resource', {i.code for i in report.issues})
        self.assertIn('missing-document-fragment', {i.code for i in report.issues})
        resource = next(i for i in report.issues if i.subject.endswith('bird one.png'))
        self.assertEqual(resource.origins[0].context.filename, 'manual-source.html')
        self.assertEqual(resource.origins[0].location.line, 2)
        self.assertEqual(before, {p.relative_to(path.parent): p.read_bytes() for p in path.parent.rglob('*') if p.is_file()})
        self.assertFalse(path.exists())
        with self.assertRaises(mkd.Error):
            product.finish()
        self.assertEqual(report.describe(), product.preflight().describe())

    def test_nested_documents_can_use_packed_resources_and_intentional_external_urls(self):
        path = self.workspace() / 'SharedChecks'
        product = new_product(path)
        dictionary = new_dictionary(product)
        target = one_birb(dictionary)
        dictionary.add_graphic('bird.png', b'packed image')
        dictionary.add_asset('manuals/guide.html', f'<html><p id=reading>Guide</p>'
            f'<img src="../graphics/bird.png"><a href="{target}-1000">Bird</a>'
            '<a href="https://example.invalid/intentional">External</a></html>'.encode())
        dictionary.add_appendix('manuals/guide.html#reading', 'Guide')
        dictionary.finish()
        report = product.preflight()
        self.assertTrue(report.valid, report.describe())
        product.finish()
        report = mkd.validate_package(path)
        self.assertTrue(report.valid, report.describe())

    def test_scanning_unregistered_documents_is_opt_in_before_and_after_publication(self):
        path = self.workspace() / 'ScanScope'
        product = new_product(path)
        dictionary = new_dictionary(product)
        one_birb(dictionary)
        dictionary.add_asset('unregistered.html', b'<html><img src="missing.png"></html>')
        dictionary.finish()
        self.assertTrue(product.preflight().valid)
        options = mkd.ValidationOptions(scan_all_documents=True)
        self.assertFalse(product.preflight(options=options).valid)
        product.finish()
        self.assertTrue(mkd.validate_package(path).valid)
        self.assertFalse(mkd.validate_package(path, options=options).valid)

    def test_tampered_document_errors_survive_report_truncation(self):
        path = self.workspace() / 'Tampered'
        product = new_product(path)
        dictionary = new_dictionary(product)
        one_birb(dictionary)
        dictionary.add_asset('manual.html', b'<html/>')
        dictionary.add_appendix('manual.html', 'Manual')
        dictionary.finish()
        product.finish()
        (path / 'Contents/birbs/manual.html').write_text('<html><img src="lost.png"><a href="#absent">Broken</a></html>')
        report = mkd.validate_package(path)
        self.assertEqual({i.code for i in report.issues if i.severity == mkd.IssueSeverity.ERROR},
                         {'missing-document-resource', 'missing-document-fragment'})
        limited = mkd.validate_package(path, options=mkd.ValidationOptions(max_issues=0))
        self.assertFalse(limited.valid)
        self.assertEqual(limited.errors, report.errors)
        self.assertTrue(limited.truncated)

    def test_native_profile_warnings_do_not_block_completion_unless_requested(self):
        path = self.workspace() / 'Warnings'
        product = new_product(path)
        dictionary = new_dictionary(product)
        one_birb(dictionary)
        report = dictionary.preflight()
        self.assertTrue(report.valid)
        self.assertGreater(report.warnings, 0)
        self.assertEqual(report.errors, 0)
        strict = dictionary.preflight(options=mkd.ValidationOptions(warnings_as_errors=True, max_issues=0))
        self.assertFalse(strict.valid)
        self.assertGreater(strict.errors, 0)
        dictionary.finish()
        product.finish()
        self.assertTrue(mkd.validate_package(path).valid)
        self.assertFalse(mkd.validate_package(path, options=mkd.ValidationOptions(warnings_as_errors=True)).valid)

    def test_qualified_article_links_resolve_against_the_named_dictionary(self):
        path = self.workspace() / 'Qualified'
        product = new_product(path)
        first = new_dictionary(product)
        second = new_dictionary(product, 'test.second', 'second', dictionary_id=2000)
        entry = first.create_entry()
        entry.write_xml('<html><body><a href="00007-0000-2000">Other dictionary</a></body></html>')
        first.add_headline(mkd.HeadlineComponents('First', entry.target))
        first.add_search_key(mkd.SearchScope.HEADWORD, 'first', entry.target)
        one_birb(second, page_id=7)
        first.finish()
        second.finish()
        report = product.preflight()
        self.assertTrue(report.valid, report.describe())
        product.finish()
        report = mkd.validate_package(path)
        self.assertTrue(report.valid, report.describe())

    def test_product_information_and_navigation_share_document_checks(self):
        path = self.workspace() / 'Information'
        product = new_product(path)
        dictionary = new_dictionary(product)
        one_birb(dictionary)
        dictionary.finish()
        product.add_asset('legal/terms.html', b'<html>\n<img src="missing.png"></html>',
                          source=mkd.SourceContext(filename='terms-source.html', record='licence'))
        product.add_asset('manual.pdf', b'%PDF fixture')
        product.set_information(mkd.ProductInformation(licenses=[mkd.ProductLicense('Terms', page='legal/terms.html')]))
        product.set_navigation(mkd.NavigationIndex(sections=[mkd.NavigationSection('Manuals', [
            mkd.NavigationItem(label='PDF', target=mkd.AppendixDocument('manual.pdf', fragment='page')),
        ])]))
        report = product.preflight()
        self.assertFalse(report.valid)
        image = next(i for i in report.issues if i.subject == 'legal/missing.png')
        self.assertEqual(image.origins[0].context.filename, 'terms-source.html')
        self.assertEqual(image.origins[0].location.line, 2)
        self.assertIn('unsupported-document-fragment', {i.code for i in report.issues})
        product.add_asset('legal/missing.png', b'image')
        product.set_navigation(mkd.NavigationIndex(sections=[mkd.NavigationSection('Manuals', [
            mkd.NavigationItem(label='PDF', target=mkd.AppendixDocument('manual.pdf')),
        ])]))
        report = product.preflight()
        self.assertTrue(report.valid, report.describe())
        product.finish()
        report = mkd.validate_package(path)
        self.assertTrue(report.valid, report.describe())

    def test_shared_fragment_references_do_not_treat_article_pages_or_css_as_html_assets(self):
        path = self.workspace() / 'Fragments'
        product = new_product(path)
        dictionary = new_dictionary(product)
        entry = dictionary.create_entry()
        entry.write_xml('<html><body><p id="reading">Read</p><a href="#reading">Local</a>'
                        '<a href="#">Top</a><p style="fill:url(#paint)">Paint</p></body></html>')
        dictionary.add_headline(mkd.HeadlineComponents('Bird', entry.target))
        dictionary.add_search_key(mkd.SearchScope.HEADWORD, 'bird', entry.target)
        dictionary.add_asset('manual.html', b'<html><p id=reading>Read</p><a href="#reading">Local</a>'
                                          b'<a href="#">Top</a><link rel=stylesheet href=manual.css></html>')
        dictionary.add_asset('manual.css', b'p { fill: url(#paint); }')
        dictionary.add_appendix('manual.html#reading', 'Manual')
        report = product.preflight()
        self.assertTrue(report.valid, report.describe())
        dictionary.finish()
        product.finish()
        report = mkd.validate_package(path)
        self.assertTrue(report.valid, report.describe())

    def test_zero_and_missing_entry_dictionary_use_current_article_context(self):
        path = self.workspace() / 'EntryContext'
        product = new_product(path)
        first = new_dictionary(product)
        one_birb(first)
        second = new_dictionary(product, 'test.second', 'second', dictionary_id=2000)
        entry = second.create_entry(page_id=7)
        entry.write_xml('<html><body><a href="00007-0000">Local</a>'
                        '<a href="00007-0000-0">Explicit zero</a>'
                        '<a href="#00007-0000-0">Numeric fragment</a></body></html>')
        second.add_headline(mkd.HeadlineComponents('Second', entry.target))
        second.add_search_key(mkd.SearchScope.HEADWORD, 'second', entry.target)
        first.finish()
        second.finish()
        report = product.preflight()
        self.assertTrue(report.valid, report.describe())
        product.finish()
        report = mkd.validate_package(path)
        self.assertTrue(report.valid, report.describe())

    def test_external_entry_dictionaries_warn_but_missing_in_package_entries_fail(self):
        path = self.workspace() / 'ExternalEntry'
        product = new_product(path)
        dictionary = new_dictionary(product)
        entry = dictionary.create_entry()
        entry.write_xml('<html><body><a href="00009-F001-9000">External dictionary</a>'
                        '<a href="#00009-F001-9000">External numeric fragment</a></body></html>')
        dictionary.add_headline(mkd.HeadlineComponents('Bird', entry.target))
        dictionary.add_search_key(mkd.SearchScope.HEADWORD, 'bird', entry.target)
        dictionary.add_asset('manual.html', b'<html><a href="00009-F001-9000">External</a></html>')
        dictionary.add_appendix('manual.html', 'Manual')
        dictionary.finish()
        report = product.preflight()
        self.assertTrue(report.valid, report.describe())
        self.assertIn('unverifiable-entry-dictionary', {i.code for i in report.issues})
        self.assertFalse(product.preflight(options=mkd.ValidationOptions(warnings_as_errors=True)).valid)
        product.finish()
        reopened = mkd.validate_package(path)
        self.assertTrue(reopened.valid, reopened.describe())
        self.assertIn('unverifiable-entry-dictionary', {i.code for i in reopened.issues})
        (path / 'Contents/birbs/manual.html').write_text('<html><a href="00009-F001-1000">Missing local entry</a></html>')
        broken = mkd.validate_package(path)
        self.assertFalse(broken.valid)
        self.assertIn('missing-entry-target', {i.code for i in broken.issues})

    def test_loose_html_has_no_implicit_dictionary_even_inside_a_dictionary_directory(self):
        for suffix in ('', '-0', '-1000'):
            with self.subTest(suffix=suffix):
                path = self.workspace() / 'LooseContext'
                product = new_product(path)
                dictionary = new_dictionary(product)
                target = one_birb(dictionary)
                dictionary.add_asset('manual.html', f'<html><a href="{target}{suffix}">Bird</a></html>'.encode())
                dictionary.add_appendix('manual.html', 'Manual')
                dictionary.finish()
                report = product.preflight()
                self.assertEqual(report.valid, suffix == '-1000', report.describe())
                if suffix != '-1000':
                    self.assertIn('missing-entry-dictionary', {i.code for i in report.issues})
                else:
                    product.finish()
                    report = mkd.validate_package(path)
                    self.assertTrue(report.valid, report.describe())

    def test_native_diagnostic_codes_agree_before_and_after_publication(self):
        path = self.workspace() / 'DiagnosticCodes'
        product = new_product(path)
        dictionary = new_dictionary(product)
        entry = dictionary.create_entry()
        entry.write_xml('<html xmlns="http://www.w3.org/1999/xhtml" hmhtml="5">'
                        '<head><style>p { color:red; }</style></head><body/></html>')
        dictionary.add_headline(mkd.HeadlineComponents('Bird', entry.target))
        dictionary.add_search_key(mkd.SearchScope.HEADWORD, 'bird', entry.target)
        authored = dictionary.preflight()
        dictionary.finish()
        product.finish()
        reopened = mkd.validate_package(path)
        self.assertEqual([i.code for i in authored.issues], ['document-coverage'])
        self.assertEqual([i.code for i in reopened.issues], ['document-coverage'])
        self.assertTrue(authored.valid)
        self.assertTrue(reopened.valid)

    def test_incomplete_document_identifiers_make_missing_fragments_advisory(self):
        path = self.workspace() / 'IncompleteIdentifiers'
        product = new_product(path)
        dictionary = new_dictionary(product)
        one_birb(dictionary)
        dictionary.add_asset('manual.html', b'<html><p id="caf&eacute;">Text</p></html>')
        dictionary.add_appendix('manual.html#absent', 'Manual')
        dictionary.finish()
        authored = product.preflight()
        self.assertTrue(authored.valid, authored.describe())
        product.finish()
        reopened = mkd.validate_package(path)
        self.assertTrue(reopened.valid, reopened.describe())
        for report in (authored, reopened):
            issue, = [i for i in report.issues if i.code == 'missing-document-fragment']
            self.assertEqual(issue.severity, mkd.IssueSeverity.WARNING)

    def test_document_dependency_aggregation_keeps_counts_and_bounded_origins(self):
        path = self.workspace() / 'DocumentOrigins'
        product = new_product(path)
        dictionary = new_dictionary(product)
        one_birb(dictionary)
        dictionary.add_asset('a.html', b'<html>' + b'<a href="#missing">Broken</a>' * 7 + b'</html>')
        dictionary.add_asset('b.html', b'<html><a href="a.html#missing">Another source</a></html>')
        dictionary.add_appendix('a.html', 'First')
        dictionary.add_appendix('b.html', 'Second')
        dictionary.finish()
        report = product.preflight()
        issue, = [i for i in report.issues if i.code == 'missing-document-fragment']
        self.assertEqual(issue.occurrences, 8)
        self.assertEqual(len(issue.origins), 3)
        self.assertFalse(report.valid)
