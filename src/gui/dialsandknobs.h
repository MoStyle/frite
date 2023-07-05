/*
 * SPDX-FileCopyrightText: 2010 Forrester Cole (fcole@cs.princeton.edu)
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _DIALS_AND_KNOBS_H_
#define _DIALS_AND_KNOBS_H_

#include <QtWidgets>
#include <QDomElement>
#include <QDomDocument>

class QGridLayout;
class QVBoxLayout;
class QEvent;
class QMenu;
class QMenuBar;
class QFileSystemModel;
class QListView;

class DialsAndKnobs;

class Tool;

typedef enum
{
    DK_PANEL,
    DK_MENU,
    DK_NUM_LOCATIONS
} dkLocation;


// dkValue
// Root class for all the types.

class dkValue : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString _name READ name)
    Q_PROPERTY(bool _is_sticky READ isSticky)

  public:
    dkValue(const QString& name, dkLocation location);
    virtual ~dkValue();

    virtual bool load(QDomElement& root) = 0;
    virtual bool save(QDomDocument& doc, QDomElement& root) const = 0;
    virtual void setFromVariant(const QVariant& v) = 0;
    virtual QVariant toVariant() const = 0;

    const QString& name() const { return _name; }
    QString scriptName() const;
    dkLocation location() const { return _location; }
    bool changedLastFrame() const;
    bool isSticky() const { return _is_sticky; }
    void setSticky(bool sticky);
    
    static dkValue* find(const QString& name);
    static QList<dkValue*> allValues();
    static int numValues() { return values().size(); }

  protected:
    static void add(dkValue* value);
    static void remove(dkValue* value);
    static QList<dkValue*>& values();
    static QHash<QString, dkValue*>& values_hash();
    
  signals:
    void stickyChanged(bool changed);

  protected:
    QString _name;
    dkLocation _location;
    int        _last_change_frame_number;
    bool       _is_sticky;

  friend class DialsAndKnobs;
};

// dkFloat
// A single double-precision floating point value.
     
class dkFloat : public dkValue
{
    Q_OBJECT
    Q_PROPERTY(double _value READ value WRITE setValue)

  public:
    dkFloat(const QString& name, double value);
    dkFloat(const QString& name, double value, 
            double lower_limit, double upper_limit, 
            double step_size);

    virtual bool load(QDomElement& root);
    virtual bool save(QDomDocument& doc, QDomElement& root) const;
    virtual void setFromVariant(const QVariant& v);
    virtual QVariant toVariant() const;

    double value() const { return _value; }
    operator double() const {return _value; }

    double lowerLimit() const { return _lower; }
    double upperLimit() const { return _upper; }
    double stepSize() const { return _step_size; }

    static dkFloat* find(const QString& name);

  public slots:
    void setValue(double f);

  signals:
    void valueChanged(double f);

  protected:
    double _value;
    double _lower, _upper;
    double _step_size;
};


class dkSlider : public dkValue
{
    Q_OBJECT
    Q_PROPERTY(int _value READ value WRITE setValue)

  public:
    dkSlider(const QString& name, int value);
    dkSlider(const QString& name, int value,
            int lower_limit, int upper_limit,
            int step_size);

    virtual bool load(QDomElement& root);
    virtual bool save(QDomDocument& doc, QDomElement& root) const;
    virtual void setFromVariant(const QVariant& v);
    virtual QVariant toVariant() const;

    int value() const { return _value; }
    operator int() const {return _value; }

    int lowerLimit() const { return _lower; }
    int upperLimit() const { return _upper; }
    int stepSize() const { return _step_size; }

    void setLowerLimit(int i);
    void setUpperLimit(int i);
    void setStepSize(int i);

    static dkSlider* find(const QString& name);

  public slots:
    void setValue(int i);

  signals:
    void valueChanged(int i);

  protected:
    int _value;
    int _lower, _upper;
    int _step_size;
};

// dkInt
// A single integer value.

class dkInt : public dkValue
{
    Q_OBJECT
    Q_PROPERTY(int _value READ value WRITE setValue)

  public:
    dkInt(const QString& name, int value);
    dkInt(const QString& name, int value, 
            int lower_limit, int upper_limit, 
            int step_size);

    virtual bool load(QDomElement& root);
    virtual bool save(QDomDocument& doc, QDomElement& root) const;
    virtual void setFromVariant(const QVariant& v);
    virtual QVariant toVariant() const;

    int value() const { return _value; }
    operator int() const {return _value; }

    int lowerLimit() const { return _lower; }
    int upperLimit() const { return _upper; }
    int stepSize() const { return _step_size; }

    static dkInt* find(const QString& name);

  public slots:
    void setValue(int i);

  signals:
    void valueChanged(int f);

  protected:
    int _value;
    int _lower, _upper;
    int _step_size;
};

// dkBool
// A single boolean value.

class dkBool : public dkValue
{
    Q_OBJECT
    Q_PROPERTY(bool _value READ value WRITE setValue)

  public:
    dkBool(const QString& name, bool value, 
           dkLocation location = DK_PANEL);

    virtual bool load(QDomElement& root);
    virtual bool save(QDomDocument& doc, QDomElement& root) const;
    virtual void setFromVariant(const QVariant& v);
    virtual QVariant toVariant() const;

    bool value() const { return _value; }
    operator bool() const {return _value; }

    static dkBool* find(const QString& name);

  public slots:
    void setValue(bool b);

  signals:
    void valueChanged(bool b);

  protected:
    bool _value;
};


// dkFilename
// A value representing a file on disk.
// !edited to select a directory instead

class dkFilename : public dkValue
{
    Q_OBJECT
    Q_PROPERTY(QString _value READ value WRITE setValue)

  public:
    dkFilename(const QString& name, const QString& value = QString());

    virtual bool load(QDomElement& root);
    virtual bool save(QDomDocument& doc, QDomElement& root) const;
    virtual void setFromVariant(const QVariant& v);
    virtual QVariant toVariant() const;

    const QString& value() const { return _value; }
    operator QString() const {return _value; }
    bool operator == (const QString& b) { return value() == b; }
    bool operator != (const QString& b) { return value() != b; }
    QByteArray toLocal8Bit() const { return value().toLocal8Bit(); }

    static dkFilename* find(const QString& name);

  public slots:
    void setValue(const QString& value);

  signals:
    void valueChanged(const QString& value);

  protected:
    QString _value;
};

// dkStringList
// A list of strings with a single selected index.

class dkStringList : public dkValue
{
    Q_OBJECT
    Q_PROPERTY(int _index READ index WRITE setIndex)

  public:
    dkStringList(const QString& name, const QStringList& choices,
                 dkLocation location = DK_PANEL);

    virtual bool load(QDomElement& root);
    virtual bool save(QDomDocument& doc, QDomElement& root) const;
    virtual void setFromVariant(const QVariant& v);
    virtual QVariant toVariant() const;

    int index() const { return _index; }
    const QString& value() const { return _string_list[_index];}
    const QStringList& stringList() const { return _string_list;}
    operator QString() const {return value();}
    bool operator == (const QString& b) { return value() == b; }
    bool operator != (const QString& b) { return value() != b; }
    QByteArray toLocal8Bit() const { return value().toLocal8Bit(); }

    void setChoices(const QStringList& choices);
    void clear() {_string_list.clear(); }

    static dkStringList* find(const QString& name);

  public slots:
    void setIndex(int index);

  signals:
    void indexChanged(int index);

  protected:
    int _index;
    QStringList _string_list;
    QSignalMapper *_signal_mapper;

  friend class DialsAndKnobs;
};

// dkImageBrowser
// A gallery of thumbnail images with the ability to pick a single image.

class dkImageBrowser : public dkValue
{
    Q_OBJECT

  public:
    dkImageBrowser(const QString& name, const QString& dir);

    virtual bool load(QDomElement& root);
    virtual bool save(QDomDocument& doc, QDomElement& root) const;
    virtual void setFromVariant(const QVariant& v) { Q_UNUSED(v) }
    virtual QVariant toVariant() const { return QVariant(); }

    QString filename() const;
    operator QString() const {return filename();}
    bool operator == (const QString& b) { return filename() == b; }
    bool operator != (const QString& b) { return filename() != b; }
    QByteArray toLocal8Bit() const { return filename().toLocal8Bit(); }

    void setModelAndView(QFileSystemModel* model, QListView* view);

    static dkImageBrowser* find(const QString& name);

  public slots:
    void itemClicked(const QModelIndex& index);

  signals:
    void selectionChanged(const QModelIndex& index);

  protected:
    QString     _root_dir;
    QFileSystemModel* _model;
    QListView* _view;

  friend class DialsAndKnobs;
};

// dkText
// A length of editable text. 

class dkText : public dkValue
{
    Q_OBJECT
    Q_PROPERTY(QString _value READ value WRITE setValue)

  public:
    dkText(const QString& name, int lines, 
           const QString& value = QString());

    virtual bool load(QDomElement& root);
    virtual bool save(QDomDocument& doc, QDomElement& root) const;
    virtual void setFromVariant(const QVariant& v);
    virtual QVariant toVariant() const;

    QString value() const { return _value; }
    operator QString() const {return _value; }
    bool operator == (const QString& b) { return value() == b; }
    bool operator != (const QString& b) { return value() != b; }
    QByteArray toLocal8Bit() const { return value().toLocal8Bit(); }

    static dkText* find(const QString& name);

  public slots:
    void setValue(const QString& value);

  signals:
    void valueChanged(const QString& value);

  protected:
    QString _value;
    int     _num_lines;
};

class DialsAndKnobsValues : public QHash<QString,QVariant>
{
  public:
    QDomElement domElement(const QString& name, QDomDocument& doc);
    void setFromDomElement(const QDomElement& element);
};

// DialsAndKnobs
// Holds pointers to each value, but *does not* own the pointers.
// Owns widgets for all the values.
class DialsAndKnobs : public QDockWidget
{
    Q_OBJECT

  public:
    DialsAndKnobs(QMainWindow* parent, QMenu* window_menu,
                  QStringList top_categories = QStringList());
    ~DialsAndKnobs();
    virtual bool event(QEvent* e);
    const QHash<QString, QDockWidget*> &newDocks() const { return _dock_widgets; }

    bool load(const QString& filename);
    bool load(const QDomElement& root, bool set_sticky = false);
    bool save(const QString& filename) const;
    bool save(QDomDocument& doc, QDomElement& root, 
              bool only_sticky = false, int version = 0) const;

    QDomElement domElement(const QString& name, QDomDocument& doc, 
                           bool only_sticky = false) const;

    QByteArray saveState(int version = 0);
    bool restoreState(const QByteArray& state, int version = 0);
    
    DialsAndKnobsValues values();
    DialsAndKnobsValues changedValues();
    void applyValues(const DialsAndKnobsValues& values);

    static int frameCounter() { return _frame_counter; }
    static void incrementFrameCounter() { _frame_counter++; }
    static void notifyUpdateLayout();
    static QString splitGroup(const QString& path);
    static QString splitBase(const QString& path);

  signals:
    void dataChanged();

  public slots:
    void toggleCategory(Tool *tool);

  protected slots:
    void updateLayout();
    void dkValueChanged();

  protected:
    void addFloatWidgets(dkFloat* dk_float);
    void addIntWidgets(dkInt* dk_int);
    void addBoolWidgets(dkBool* dk_bool);
    void addFilenameWidgets(dkFilename* dk_filename);
    void addStringListWidgets(dkStringList* dk_string_list);
    void addImageBrowserWidgets(dkImageBrowser* dk_image_browser);
    void addTextWidgets(dkText* dk_text);
    void addSliderWidgets(dkSlider* dk_slider);
    
    QMenu* findOrCreateMenu(const QString& group);
    QGridLayout* findOrCreateLayout(const QString& group);
    
  protected:
    QGridLayout* _root_layout;
    QMenuBar* _parent_menu_bar;
    QMenu* _parent_window_menu;
    QHash<QString, QMenu*> _menus;
    QHash<QString, QGridLayout*> _layouts;
    QHash<QString, QDockWidget*> _dock_widgets;
    bool _in_load;

    static int _frame_counter;
    static DialsAndKnobs* _instance;
};


#endif // _DIALS_AND_KNOBS_H_
