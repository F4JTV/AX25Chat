/*
 * SymbolPicker.h - APRS symbol chooser and the symbol name tables.
 *
 * Port of ax25chat/ui/symbol_picker.py and the tables of aprs.py.  A symbol
 * is two characters: a table identifier and a symbol code.  '/' selects the
 * primary table, '\' the alternate table, and a digit or capital letter
 * selects the alternate table with that character drawn over the icon.
 * The descriptions follow the APRS Protocol Reference 1.0.1 Appendix 2; the
 * two-character code is the authoritative part.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace symbols {

struct Entry { QString code; QString description; };
struct Common { QString table; QString code; QString description; };

// (code, description) pairs for '/' or '\', in code order.
const QList<Entry> &tableEntries(const QString &table);
// The shortlist an amateur station realistically uses.
const QList<Common> &commonSymbols();
// Conventional name of a symbol, including overlaid ones.
QString description(const QString &table, const QString &code);
bool inTable(const QString &table, const QString &code);

} // namespace symbols


class SymbolPicker : public QDialog
{
    Q_OBJECT

public:
    explicit SymbolPicker(const QString &table = QStringLiteral("/"), const QString &code = QStringLiteral("-"),
                          QWidget *parent = nullptr);

    QString table() const { return m_table; }
    QString code() const { return m_code; }

    static QString describe(const QString &table, const QString &code) { return symbols::description(table, code); }

private:
    int initialScopeIndex() const;
    void repopulate();
    void onSelection(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void onOverlayToggled(bool checked);
    void onOverlayChanged(const QString &text);
    void refreshIcons();
    void updatePreview();

    QString m_table;
    QString m_code;
    QComboBox *m_scopeCombo = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QTreeWidget *m_tree = nullptr;
    QCheckBox *m_overlayCheck = nullptr;
    QLineEdit *m_overlayEdit = nullptr;
    QLabel *m_previewIcon = nullptr;
    QLabel *m_previewLabel = nullptr;
    QLabel *m_warningLabel = nullptr;
};
