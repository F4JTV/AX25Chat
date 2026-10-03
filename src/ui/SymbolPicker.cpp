/*
 * SymbolPicker.cpp - APRS symbol chooser and the symbol name tables.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SymbolPicker.h"

#include "AprsEncoder.h"
#include "Style.h"
#include "SymbolArt.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace symbols {

namespace {

const QList<Entry> &primaryTable()
{
    static const QList<Entry> table = {
        {"!", "Police station"},
        {"\"", "Reserved"},
        {"#", "Digipeater"},
        {"$", "Telephone"},
        {"%", "DX cluster"},
        {"&", "HF gateway"},
        {"'", "Aircraft, small"},
        {"(", "Mobile satellite station"},
        {")", "Wheelchair"},
        {"*", "Snowmobile"},
        {"+", "Red Cross"},
        {",", "Boy Scouts"},
        {"-", "House, QTH (VHF)"},
        {".", "X, unknown position"},
        {"/", "Red dot"},
        {"0", "Circle, numbered 0"},
        {"1", "Circle, numbered 1"},
        {"2", "Circle, numbered 2"},
        {"3", "Circle, numbered 3"},
        {"4", "Circle, numbered 4"},
        {"5", "Circle, numbered 5"},
        {"6", "Circle, numbered 6"},
        {"7", "Circle, numbered 7"},
        {"8", "Circle, numbered 8"},
        {"9", "Circle, numbered 9"},
        {":", "Fire"},
        {";", "Campground, portable"},
        {"<", "Motorcycle"},
        {"=", "Railroad engine"},
        {">", "Car"},
        {"?", "File server"},
        {"@", "Hurricane, tropical storm"},
        {"A", "Aid station"},
        {"B", "BBS"},
        {"C", "Canoe"},
        {"D", "Reserved"},
        {"E", "Eyeball, operator present"},
        {"F", "Farm vehicle, tractor"},
        {"G", "Grid square, 3 digit"},
        {"H", "Hotel"},
        {"I", "TCP/IP network station"},
        {"J", "Reserved"},
        {"K", "School"},
        {"L", "PC user, logged on"},
        {"M", "MacAPRS"},
        {"N", "NTS station"},
        {"O", "Balloon"},
        {"P", "Police"},
        {"Q", "Quake"},
        {"R", "Recreational vehicle"},
        {"S", "Space shuttle, satellite"},
        {"T", "SSTV"},
        {"U", "Bus"},
        {"V", "ATV"},
        {"W", "Weather service site"},
        {"X", "Helicopter"},
        {"Y", "Yacht, sailboat"},
        {"Z", "WinAPRS"},
        {"[", "Person, walker"},
        {"\\", "DF station, triangle"},
        {"]", "Mail, post office"},
        {"^", "Aircraft, large"},
        {"_", "Weather station"},
        {"`", "Dish antenna"},
        {"a", "Ambulance"},
        {"b", "Bicycle"},
        {"c", "Incident command post"},
        {"d", "Fire department"},
        {"e", "Horse, equestrian"},
        {"f", "Fire truck"},
        {"g", "Glider"},
        {"h", "Hospital"},
        {"i", "IOTA, island"},
        {"j", "Jeep"},
        {"k", "Truck"},
        {"l", "Laptop"},
        {"m", "Mic-E repeater"},
        {"n", "Node"},
        {"o", "Emergency operations centre"},
        {"p", "Dog, rover"},
        {"q", "Grid square, large"},
        {"r", "Repeater"},
        {"s", "Ship, power boat"},
        {"t", "Truck stop"},
        {"u", "Truck, 18 wheeler"},
        {"v", "Van"},
        {"w", "Water station"},
        {"x", "Unix workstation"},
        {"y", "Yagi at QTH"},
        {"z", "Reserved"},
        {"{", "Reserved"},
        {"|", "Reserved, TNC stream switch"},
        {"}", "Reserved"},
        {"~", "Reserved, TNC stream switch"},
    };
    return table;
}

const QList<Entry> &alternateTable()
{
    static const QList<Entry> table = {
        {"!", "Emergency"},
        {"\"", "Reserved"},
        {"#", "Star, overlay digit"},
        {"$", "Bank, ATM"},
        {"%", "Reserved"},
        {"&", "Gateway, overlay"},
        {"'", "Crash site, incident"},
        {"(", "Cloudy"},
        {")", "Firenet MEO, MODIS"},
        {"*", "Snow"},
        {"+", "Church"},
        {",", "Girl Scouts"},
        {"-", "House, HF operator"},
        {".", "Ambiguous position"},
        {"/", "Waypoint destination"},
        {"0", "Circle overlay 0"},
        {"1", "Circle overlay 1"},
        {"2", "Circle overlay 2"},
        {"3", "Circle overlay 3"},
        {"4", "Circle overlay 4"},
        {"5", "Circle overlay 5"},
        {"6", "Circle overlay 6"},
        {"7", "Circle overlay 7"},
        {"8", "Circle overlay 8"},
        {"9", "Circle overlay 9"},
        {":", "Hail"},
        {";", "Park, picnic area"},
        {"<", "Advisory, gale flag"},
        {"=", "Reserved"},
        {">", "Car, overlay"},
        {"?", "Information kiosk"},
        {"@", "Hurricane"},
        {"A", "Box, overlay"},
        {"B", "Blowing snow"},
        {"C", "Coast guard"},
        {"D", "Drizzle"},
        {"E", "Smoke"},
        {"F", "Freezing rain"},
        {"G", "Snow shower"},
        {"H", "Haze"},
        {"I", "Rain shower"},
        {"J", "Lightning"},
        {"K", "Kenwood handheld"},
        {"L", "Lighthouse"},
        {"M", "Reserved"},
        {"N", "Navigation buoy"},
        {"O", "Rocket"},
        {"P", "Parking"},
        {"Q", "Earthquake"},
        {"R", "Restaurant"},
        {"S", "Satellite"},
        {"T", "Thunderstorm"},
        {"U", "Sunny"},
        {"V", "VORTAC navigation aid"},
        {"W", "Weather service site"},
        {"X", "Pharmacy"},
        {"Y", "Reserved"},
        {"Z", "Reserved"},
        {"[", "Wall cloud"},
        {"\\", "Reserved"},
        {"]", "Reserved"},
        {"^", "Aircraft, overlay"},
        {"_", "Weather site, overlay"},
        {"`", "Rain"},
        {"a", "ARRL, ARES, WinLink"},
        {"b", "Blowing dust"},
        {"c", "CD triangle, overlay"},
        {"d", "DX spot"},
        {"e", "Sleet"},
        {"f", "Funnel cloud"},
        {"g", "Gale flags"},
        {"h", "Store, ham shop"},
        {"i", "Point of interest"},
        {"j", "Work zone"},
        {"k", "SUV, special vehicle"},
        {"l", "Areas"},
        {"m", "Value sign"},
        {"n", "Triangle, overlay"},
        {"o", "Small circle"},
        {"p", "Partly cloudy"},
        {"q", "Reserved"},
        {"r", "Restrooms"},
        {"s", "Ship, overlay"},
        {"t", "Tornado"},
        {"u", "Truck, overlay"},
        {"v", "Van, overlay"},
        {"w", "Flooding"},
        {"x", "Wreck, obstruction"},
        {"y", "Skywarn"},
        {"z", "Shelter, overlay"},
    };
    return table;
}

const Entry *find(const QList<Entry> &table, const QString &code)
{
    for (const Entry &e : table) if (e.code == code) return &e;
    return nullptr;
}

} // namespace

const QList<Entry> &tableEntries(const QString &table)
{
    return table == QStringLiteral("/") ? primaryTable() : alternateTable();
}

const QList<Common> &commonSymbols()
{
    static const QList<Common> list = {
        {"/", "-", "House, QTH (VHF)"},
        {"/", ">", "Car"},
        {"/", "k", "Truck"},
        {"/", "j", "Jeep"},
        {"/", "v", "Van"},
        {"/", "b", "Bicycle"},
        {"/", "<", "Motorcycle"},
        {"/", "[", "Person, walker"},
        {"/", "R", "Recreational vehicle"},
        {"/", "Y", "Yacht, sailboat"},
        {"/", "s", "Ship, power boat"},
        {"/", "'", "Aircraft, small"},
        {"/", "O", "Balloon"},
        {"/", "_", "Weather station"},
        {"/", "I", "TCP/IP network station"},
        {"/", "#", "Digipeater"},
        {"/", "&", "HF gateway"},
        {"/", "r", "Repeater"},
        {"/", "E", "Eyeball, operator present"},
        {"/", ";", "Campground, portable"},
        {"/", "y", "Yagi at QTH"},
        {"\\", "-", "House, HF operator"},
        {"\\", "!", "Emergency"},
        {"\\", "+", "Church"},
        {"\\", "a", "ARRL, ARES, WinLink"},
        {"\\", "f", "Funnel cloud"},
        {"\\", "h", "Store, ham shop"},
        {"\\", "t", "Tornado"},
        {"\\", "w", "Flooding"},
    };
    return list;
}

QString description(const QString &table, const QString &code)
{
    if (aprs::isOverlay(table)) {
        const Entry *e = find(alternateTable(), code);
        return QStringLiteral("%1, overlay '%2'").arg(e ? e->description : QStringLiteral("Unknown symbol"), table);
    }
    const Entry *e = find(table == QStringLiteral("/") ? primaryTable() : alternateTable(), code);
    return e ? e->description : QStringLiteral("Unknown symbol");
}

bool inTable(const QString &table, const QString &code)
{
    return find(table == QStringLiteral("/") ? primaryTable() : alternateTable(), code) != nullptr;
}

} // namespace symbols


SymbolPicker::SymbolPicker(const QString &table, const QString &code, QWidget *parent)
    : QDialog(parent), m_table(table.isEmpty() ? QStringLiteral("/") : table), m_code(code.isEmpty() ? QStringLiteral("-") : code)
{
    setWindowTitle(QStringLiteral("Choose a map symbol"));
    resize(560, 520);
    auto *layout = new QVBoxLayout(this);

    auto *top = new QHBoxLayout;
    m_scopeCombo = new QComboBox(this);
    m_scopeCombo->addItem(QStringLiteral("Common amateur symbols"), QStringLiteral("common"));
    m_scopeCombo->addItem(QStringLiteral("Primary table  /"), QStringLiteral("/"));
    m_scopeCombo->addItem(QStringLiteral("Alternate table  \\"), QStringLiteral("\\"));
    m_scopeCombo->addItem(QStringLiteral("Everything"), QStringLiteral("all"));
    top->addWidget(m_scopeCombo);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("Search by name or code"));
    m_searchEdit->setClearButtonEnabled(true);
    top->addWidget(m_searchEdit, 1);
    layout->addLayout(top);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({QStringLiteral("Code"), QStringLiteral("Table"), QStringLiteral("Description")});
    m_tree->setRootIsDecorated(false);
    m_tree->setIconSize(QSize(28, 28));
    m_tree->header()->setStretchLastSection(true);
    layout->addWidget(m_tree, 1);

    auto *overlayRow = new QHBoxLayout;
    m_overlayCheck = new QCheckBox(QStringLiteral("Overlay a character"), this);
    overlayRow->addWidget(m_overlayCheck);
    m_overlayEdit = new QLineEdit(this);
    m_overlayEdit->setMaxLength(1);
    m_overlayEdit->setFixedWidth(40);
    m_overlayEdit->setEnabled(false);
    overlayRow->addWidget(m_overlayEdit);
    auto *overlayHint = new QLabel(QStringLiteral("A digit or capital letter drawn over an alternate-table icon, to tell "
                                                  "apart stations sharing a symbol."), this);
    overlayHint->setWordWrap(true);
    style::applyHintStyle(overlayHint);
    overlayRow->addWidget(overlayHint, 1);
    layout->addLayout(overlayRow);

    auto *previewRow = new QHBoxLayout;
    m_previewIcon = new QLabel(this);
    m_previewIcon->setFixedSize(64, 64);
    previewRow->addWidget(m_previewIcon);
    m_previewLabel = new QLabel(this);
    m_previewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    previewRow->addWidget(m_previewLabel, 1);
    layout->addLayout(previewRow);

    m_warningLabel = new QLabel(this);
    m_warningLabel->setWordWrap(true);
    style::applyHintStyle(m_warningLabel);
    layout->addWidget(m_warningLabel);

    auto *attribution = new QLabel(symbolart::available() ? symbolart::attribution()
                                                          : QStringLiteral("Symbol artwork not installed: run scripts/fetch_symbols.sh "
                                                                           "to show icons. Names and codes work without it."), this);
    attribution->setWordWrap(true);
    style::applyHintStyle(attribution);
    layout->addWidget(attribution);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    if (aprs::isOverlay(m_table)) {
        m_overlayCheck->setChecked(true);
        m_overlayEdit->setEnabled(true);
        m_overlayEdit->setText(m_table);
    }
    m_scopeCombo->setCurrentIndex(initialScopeIndex());

    connect(m_scopeCombo, &QComboBox::currentIndexChanged, this, [this](int) { repopulate(); });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &) { repopulate(); });
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &SymbolPicker::onSelection);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *, int) { accept(); });
    connect(m_overlayCheck, &QCheckBox::toggled, this, &SymbolPicker::onOverlayToggled);
    connect(m_overlayEdit, &QLineEdit::textChanged, this, &SymbolPicker::onOverlayChanged);
    repopulate();
}

// Start on the scope that contains the current symbol.
int SymbolPicker::initialScopeIndex() const
{
    const QString baseTable = aprs::isOverlay(m_table) ? QStringLiteral("\\") : m_table;
    for (const symbols::Common &c : symbols::commonSymbols()) {
        if (c.table == baseTable && c.code == m_code) return 0;
    }
    return baseTable == QStringLiteral("/") ? 1 : 2;
}

void SymbolPicker::repopulate()
{
    const QString scope = m_scopeCombo->currentData().toString();
    const QString needle = m_searchEdit->text().trimmed().toLower();

    QList<symbols::Common> rows;
    if (scope == QLatin1String("common")) {
        rows = symbols::commonSymbols();
    } else {
        const QStringList tables = scope == QLatin1String("all") ? QStringList{QStringLiteral("/"), QStringLiteral("\\")} : QStringList{scope};
        for (const QString &table : tables) {
            for (const symbols::Entry &e : symbols::tableEntries(table)) {
                if (e.description == QLatin1String("Reserved")) continue;   // not selectable
                rows.append({table, e.code, e.description});
            }
        }
    }
    if (!needle.isEmpty()) {
        QList<symbols::Common> filtered;
        for (const symbols::Common &r : rows) {
            if (r.description.toLower().contains(needle) || r.code.toLower() == needle) filtered.append(r);
        }
        rows = filtered;
    }

    m_tree->blockSignals(true);
    m_tree->clear();
    QTreeWidgetItem *selected = nullptr;
    const bool overlayActive = m_overlayCheck->isChecked();
    for (const symbols::Common &r : rows) {
        auto *item = new QTreeWidgetItem({r.code, r.table == QStringLiteral("/") ? QStringLiteral("Primary  /") : QStringLiteral("Alternate  \\"), r.description});
        item->setData(0, Qt::UserRole, r.table);
        item->setData(0, Qt::UserRole + 1, r.code);
        // Show the symbol as it will appear, including the overlay when one
        // is active, so the list matches what other stations see.
        const QIcon art = symbolart::icon(overlayActive ? m_table : r.table, r.code, 28);
        if (!art.isNull()) item->setIcon(0, art);
        m_tree->addTopLevelItem(item);
        if (r.code == m_code && (r.table == m_table || (aprs::isOverlay(m_table) && r.table == QStringLiteral("\\")))) selected = item;
    }
    m_tree->blockSignals(false);
    if (selected) {
        m_tree->setCurrentItem(selected);
        m_tree->scrollToItem(selected);
    } else if (m_tree->topLevelItemCount()) {
        m_tree->setCurrentItem(m_tree->topLevelItem(0));
    } else {
        updatePreview();
    }
    m_tree->resizeColumnToContents(0);
    m_tree->resizeColumnToContents(1);
}

void SymbolPicker::onSelection(QTreeWidgetItem *current, QTreeWidgetItem *)
{
    if (!current) return;
    m_code = current->data(0, Qt::UserRole + 1).toString();
    if (!m_overlayCheck->isChecked()) m_table = current->data(0, Qt::UserRole).toString();
    updatePreview();
}

void SymbolPicker::onOverlayToggled(bool checked)
{
    m_overlayEdit->setEnabled(checked);
    if (checked) {
        const QString character = (m_overlayEdit->text().isEmpty() ? QStringLiteral("A") : m_overlayEdit->text()).toUpper();
        m_overlayEdit->setText(character);
        m_table = character;
    } else if (QTreeWidgetItem *current = m_tree->currentItem()) {
        m_table = current->data(0, Qt::UserRole).toString();
    } else {
        m_table = QStringLiteral("/");
    }
    updatePreview();
    refreshIcons();
}

void SymbolPicker::onOverlayChanged(const QString &text)
{
    if (!m_overlayCheck->isChecked()) return;
    const QString upper = text.toUpper();
    if (upper != text) {
        m_overlayEdit->setText(upper);
        return;
    }
    if (!upper.isEmpty()) m_table = upper;
    updatePreview();
    refreshIcons();
}

void SymbolPicker::refreshIcons()
{
    if (!symbolart::available()) return;
    const bool overlayActive = m_overlayCheck->isChecked();
    for (int i = 0; i < m_tree->topLevelItemCount(); i++) {
        QTreeWidgetItem *item = m_tree->topLevelItem(i);
        const QString table = item->data(0, Qt::UserRole).toString();
        const QString code = item->data(0, Qt::UserRole + 1).toString();
        const QIcon art = symbolart::icon(overlayActive ? m_table : table, code, 28);
        if (!art.isNull()) item->setIcon(0, art);
    }
}

void SymbolPicker::updatePreview()
{
    const QPixmap art = symbolart::pixmap(m_table, m_code, 64);
    if (!art.isNull()) m_previewIcon->setPixmap(art);
    else m_previewIcon->clear();
    m_previewLabel->setText(QStringLiteral("Symbol:  %1%2\nOn air:  !4903.50N%1" "07201.75W%2\nMeaning: %3")
                                .arg(m_table, m_code, symbols::description(m_table, m_code)));

    QStringList problems;
    if (aprs::isOverlay(m_table)) {
        // An overlay always selects the alternate table, so a primary-only
        // symbol silently becomes a different icon.
        if (!symbols::inTable(QStringLiteral("\\"), m_code)) {
            problems << QStringLiteral("'%1' is not in the alternate table, and an overlay always uses the alternate table.").arg(m_code);
        } else if (symbols::description(QStringLiteral("\\"), m_code) == QLatin1String("Reserved")) {
            problems << QStringLiteral("'%1' is reserved in the alternate table.").arg(m_code);
        }
    } else if (m_table == QStringLiteral("/") && !symbols::inTable(m_table, m_code)) {
        problems << QStringLiteral("'%1' is not in the primary table.").arg(m_code);
    } else if (m_table == QStringLiteral("\\") && !symbols::inTable(m_table, m_code)) {
        problems << QStringLiteral("'%1' is not in the alternate table.").arg(m_code);
    }
    QString error;
    if (!aprs::validateSymbol(m_table, m_code, &error)) problems << error;
    m_warningLabel->setText(problems.join('\n'));
}
