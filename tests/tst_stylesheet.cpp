#include <QtTest>

#include "app/utilities/stylesheet.hpp"

class StylesheetTest : public QObject {
    Q_OBJECT

   private Q_SLOTS:
    void scales_each_length_once()
    {
        QCOMPARE(scale_stylesheet("a { border: 2px; padding: 3px; margin: 2px; }", 1.5),
                 QString("a { border: 3px; padding: 5px; margin: 3px; }"));
    }

    void keeps_multi_value_declarations_valid()
    {
        QCOMPARE(scale_stylesheet("a { padding: 4px 8px; }", 1.0), QString("a { padding: 4px 8px; }"));
        QCOMPARE(scale_stylesheet("a { padding: 4px 8px; }", 2.0), QString("a { padding: 8px 16px; }"));
    }

    void rounds_up_and_keeps_sign()
    {
        QCOMPARE(scale_stylesheet("a { top: -10px; width: 1px; }", 1.25), QString("a { top: -12px; width: 2px; }"));
    }

    void leaves_unit_scale_and_unspaced_values_alone()
    {
        QCOMPARE(scale_stylesheet("a { border: 2px solid; }", 1.0), QString("a { border: 2px solid; }"));
        QCOMPARE(scale_stylesheet("a { border:2px; }", 2.0), QString("a { border:2px; }"));
    }
};

QTEST_GUILESS_MAIN(StylesheetTest)
#include "tst_stylesheet.moc"
