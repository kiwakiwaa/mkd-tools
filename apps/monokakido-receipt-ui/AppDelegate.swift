import AppKit

private final class FlippedView: NSView {
    override var isFlipped: Bool { true }
}

final class AppDelegate: NSObject, NSApplicationDelegate, NSWindowDelegate {
    private let bridge = MKDReceiptBridge()

    private var window: NSWindow!
    private var summaryLabel: NSTextField!
    private var generateButton: NSButton!
    private var refreshButton: NSButton!
    private var listScrollView: NSScrollView!
    private var listContainer: FlippedView!

    private var rows: [MKDReceiptDictionaryItem] = []
    private let rowHeight: CGFloat = 44
    private let rowSpacing: CGFloat = 0
    private let rowInset: CGFloat = 8

    func applicationDidFinishLaunching(_ notification: Notification) {
        do {
            try bridge.ensureAccessPrompting(true)
        } catch {
            let alert = NSAlert()
            alert.alertStyle = .warning
            alert.messageText = "Monokakido Receipt Manager"
            alert.informativeText = "Access to Monokakido dictionaries is required.\n\n\(error.localizedDescription)"
            alert.addButton(withTitle: "Quit")
            alert.runModal()
            NSApp.terminate(nil)
            return
        }

        buildMenu()
        buildUI()
        refresh()
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        true
    }

    func windowDidResize(_ notification: Notification) {
        layoutRows()
    }

    private func buildUI() {
        window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 700, height: 560),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.center()
        window.title = "Monokakido Receipt Manager"
        window.delegate = self
        window.makeKeyAndOrderFront(nil)

        guard let content = window.contentView else { return }

        let header = NSView()
        header.translatesAutoresizingMaskIntoConstraints = false
        content.addSubview(header)

        summaryLabel = NSTextField(labelWithString: "")
        summaryLabel.translatesAutoresizingMaskIntoConstraints = false
        summaryLabel.font = NSFont.systemFont(ofSize: 13, weight: .medium)
        summaryLabel.textColor = .secondaryLabelColor
        header.addSubview(summaryLabel)

        refreshButton = NSButton(image: NSImage(systemSymbolName: "arrow.clockwise", accessibilityDescription: "Refresh") ?? NSImage(), target: self, action: #selector(refreshTapped))
        refreshButton.translatesAutoresizingMaskIntoConstraints = false
        refreshButton.bezelStyle = .texturedRounded
        refreshButton.isBordered = true
        refreshButton.imagePosition = .imageOnly
        refreshButton.contentTintColor = .secondaryLabelColor
        header.addSubview(refreshButton)

        generateButton = NSButton(title: "Generate Missing", target: self, action: #selector(generateTapped))
        generateButton.translatesAutoresizingMaskIntoConstraints = false
        generateButton.bezelStyle = .rounded
        header.addSubview(generateButton)

        listScrollView = NSScrollView()
        listScrollView.translatesAutoresizingMaskIntoConstraints = false
        listScrollView.hasVerticalScroller = true
        listScrollView.drawsBackground = false
        content.addSubview(listScrollView)

        listContainer = FlippedView(frame: .zero)
        listContainer.autoresizingMask = [.width]
        listScrollView.documentView = listContainer

        NSLayoutConstraint.activate([
            header.topAnchor.constraint(equalTo: content.topAnchor, constant: 12),
            header.leadingAnchor.constraint(equalTo: content.leadingAnchor, constant: 14),
            header.trailingAnchor.constraint(equalTo: content.trailingAnchor, constant: -14),
            header.heightAnchor.constraint(equalToConstant: 30),

            summaryLabel.leadingAnchor.constraint(equalTo: header.leadingAnchor),
            summaryLabel.centerYAnchor.constraint(equalTo: header.centerYAnchor),
            summaryLabel.trailingAnchor.constraint(lessThanOrEqualTo: generateButton.leadingAnchor, constant: -12),

            generateButton.trailingAnchor.constraint(equalTo: refreshButton.leadingAnchor, constant: -8),
            generateButton.centerYAnchor.constraint(equalTo: header.centerYAnchor),

            refreshButton.trailingAnchor.constraint(equalTo: header.trailingAnchor),
            refreshButton.centerYAnchor.constraint(equalTo: header.centerYAnchor),
            refreshButton.widthAnchor.constraint(equalToConstant: 28),
            refreshButton.heightAnchor.constraint(equalToConstant: 28),

            listScrollView.topAnchor.constraint(equalTo: header.bottomAnchor, constant: 8),
            listScrollView.leadingAnchor.constraint(equalTo: content.leadingAnchor),
            listScrollView.trailingAnchor.constraint(equalTo: content.trailingAnchor),
            listScrollView.bottomAnchor.constraint(equalTo: content.bottomAnchor, constant: -10)
        ])
    }

    private func buildMenu() {
        let mainMenu = NSMenu()
        NSApp.mainMenu = mainMenu

        let appMenuItem = NSMenuItem()
        mainMenu.addItem(appMenuItem)

        let appMenu = NSMenu()
        appMenuItem.submenu = appMenu

        let appName = Bundle.main.object(forInfoDictionaryKey: "CFBundleName") as? String ?? "MKDReceiptApp"
        appMenu.addItem(NSMenuItem(title: "About \(appName)", action: #selector(showAboutPanel), keyEquivalent: ""))
        appMenu.addItem(NSMenuItem.separator())
        appMenu.addItem(NSMenuItem(title: "Hide \(appName)", action: #selector(NSApplication.hide(_:)), keyEquivalent: "h"))
        appMenu.addItem(NSMenuItem(title: "Hide Others", action: #selector(NSApplication.hideOtherApplications(_:)), keyEquivalent: "h"))
        appMenu.items.last?.keyEquivalentModifierMask = [.command, .option]
        appMenu.addItem(NSMenuItem(title: "Show All", action: #selector(NSApplication.unhideAllApplications(_:)), keyEquivalent: ""))
        appMenu.addItem(NSMenuItem.separator())
        appMenu.addItem(NSMenuItem(title: "Quit \(appName)", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q"))
    }

    @objc private func showAboutPanel() {
        let appName = Bundle.main.object(forInfoDictionaryKey: "CFBundleName") as? String ?? "MKDReceiptApp"
        let shortVersion = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "1.0"
        let buildVersion = Bundle.main.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? "1"

        let description = "Inspect and generate Monokakido receipt records for installed dictionaries."
        let author = "Author: kiwakiwaa"
        let credits = NSAttributedString(string: "\(description)\n\n\(author)")

        NSApp.orderFrontStandardAboutPanel(options: [
            .applicationName: appName,
            .applicationVersion: shortVersion,
            .version: buildVersion,
            .credits: credits
        ])
    }

    @objc private func refreshTapped() {
        refresh()
    }

    @objc private func generateTapped() {
        var nsError: NSError?
        let count = bridge.generateMissingPurchasesWithError(&nsError)
        if let nsError {
            presentErrorAlert(message: nsError.localizedDescription)
            return
        }

        if count == 0 {
            summaryLabel.stringValue = "No missing records were generated."
        }
        refresh()
    }

    private func refresh() {
        let snapshot: MKDReceiptSnapshot
        do {
            snapshot = try bridge.loadSnapshot()
        } catch {
            rows = []
            renderRows()
            summaryLabel.stringValue = error.localizedDescription
            return
        }

        rows = snapshot.items.sorted { lhs, rhs in
            if lhs.hasReceipt != rhs.hasReceipt { return lhs.hasReceipt && !rhs.hasReceipt }
            return lhs.dictionaryTitle.localizedStandardCompare(rhs.dictionaryTitle) == .orderedAscending
        }

        summaryLabel.stringValue = "\(rows.count) dictionaries · \(snapshot.missingCount) missing receipt records"
        renderRows()
    }

    private func renderRows() {
        listContainer.subviews.forEach { $0.removeFromSuperview() }

        if rows.isEmpty {
            let emptyLabel = NSTextField(labelWithString: "No dictionaries found.")
            emptyLabel.font = NSFont.systemFont(ofSize: 13)
            emptyLabel.textColor = .secondaryLabelColor
            emptyLabel.frame = NSRect(x: 16, y: 14, width: 260, height: 20)
            listContainer.addSubview(emptyLabel)
            listContainer.frame = NSRect(x: 0, y: 0, width: max(320, listScrollView.contentSize.width), height: 48)
            return
        }

        for (index, item) in rows.enumerated() {
            listContainer.addSubview(makeRow(for: item, index: index))
        }

        layoutRows()
        DispatchQueue.main.async { [weak self] in
            self?.layoutRows()
        }
    }

    private func layoutRows() {
        let viewportWidth = listScrollView.contentView.bounds.width
        let width = max(420, viewportWidth - (rowInset * 2))
        var y = rowInset
        for row in listContainer.subviews {
            row.frame = NSRect(x: rowInset, y: y, width: width, height: rowHeight)
            y += rowHeight + rowSpacing
        }
        let contentHeight = max(y + rowInset, listScrollView.contentSize.height + 1)
        listContainer.frame = NSRect(x: 0, y: 0, width: width + (rowInset * 2), height: contentHeight)
    }

    private func makeRow(for item: MKDReceiptDictionaryItem, index: Int) -> NSView {
        let row = NSView(frame: NSRect(x: 0, y: 0, width: 0, height: rowHeight))
        row.wantsLayer = true
        row.layer?.backgroundColor = NSColor.clear.cgColor

        let iconView = NSImageView()
        iconView.translatesAutoresizingMaskIntoConstraints = false
        iconView.imageScaling = .scaleProportionallyUpOrDown
        iconView.image = resolveIcon(for: item)
        row.addSubview(iconView)

        let titleLabel = NSTextField(labelWithString: item.dictionaryTitle)
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        titleLabel.font = NSFont.systemFont(ofSize: 13, weight: .medium)
        titleLabel.lineBreakMode = .byTruncatingTail
        row.addSubview(titleLabel)

        let dateLabel = NSTextField(labelWithString: formattedDate(from: item.purchaseDate))
        dateLabel.translatesAutoresizingMaskIntoConstraints = false
        dateLabel.font = NSFont.systemFont(ofSize: 12)
        dateLabel.textColor = .secondaryLabelColor
        dateLabel.alignment = .right
        row.addSubview(dateLabel)

        let separator = NSBox()
        separator.translatesAutoresizingMaskIntoConstraints = false
        separator.boxType = .separator
        separator.alphaValue = 0.35
        row.addSubview(separator)

        NSLayoutConstraint.activate([
            iconView.leadingAnchor.constraint(equalTo: row.leadingAnchor, constant: 10),
            iconView.centerYAnchor.constraint(equalTo: row.centerYAnchor),
            iconView.widthAnchor.constraint(equalToConstant: 24),
            iconView.heightAnchor.constraint(equalToConstant: 24),

            dateLabel.trailingAnchor.constraint(equalTo: row.trailingAnchor, constant: -10),
            dateLabel.centerYAnchor.constraint(equalTo: row.centerYAnchor),
            dateLabel.widthAnchor.constraint(equalToConstant: 108),

            titleLabel.leadingAnchor.constraint(equalTo: iconView.trailingAnchor, constant: 10),
            titleLabel.trailingAnchor.constraint(equalTo: dateLabel.leadingAnchor, constant: -12),
            titleLabel.centerYAnchor.constraint(equalTo: row.centerYAnchor),

            separator.leadingAnchor.constraint(equalTo: row.leadingAnchor, constant: 8),
            separator.trailingAnchor.constraint(equalTo: row.trailingAnchor, constant: -8),
            separator.bottomAnchor.constraint(equalTo: row.bottomAnchor)
        ])

        if !item.hasReceipt {
            titleLabel.textColor = .tertiaryLabelColor
            dateLabel.textColor = .tertiaryLabelColor
            iconView.alphaValue = 0.45
        }

        return row
    }

    private func resolveIcon(for item: MKDReceiptDictionaryItem) -> NSImage? {
        if let iconPath = item.iconPath, !iconPath.isEmpty, let image = NSImage(contentsOfFile: iconPath) {
            return image
        }
        return NSImage(named: NSImage.Name.folder)
    }

    private func formattedDate(from value: String?) -> String {
        guard let value, !value.isEmpty else { return "—" }
        if let tIndex = value.firstIndex(of: "T") {
            return String(value[..<tIndex])
        }
        return value
    }

    private func presentErrorAlert(message: String) {
        let alert = NSAlert()
        alert.alertStyle = .warning
        alert.messageText = "Monokakido Receipt Manager"
        alert.informativeText = message
        alert.addButton(withTitle: "OK")
        alert.beginSheetModal(for: window)
    }
}
