#include "processes.h"
#include <QStandardItem>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QMutexLocker>
#include "constants.h"
#include "utils.h"

ProcessList::ProcessList(const std::string& name, const std::string& type,
                         const std::string& gpuIdx, const std::string& pid,
                         const std::string& sm, const std::string& mem,
                         const std::string& enc, const std::string& dec,
                         const std::string& fbmem)
{
    this->name = name;
    this->type = type;
    this->gpuIdx = gpuIdx;
    this->pid = pid;
    this->sm = sm;
    this->mem = mem;
    this->enc = enc;
    this->dec = dec;
    this->fbmem = fbmem;
}

void ProcessesWorker::work() {
    mutex.lock();
    std::vector<std::string> lines = split(streamline(exec(NVSMI_CMD_PROCESSES)), "\n"), data;
    processes.clear();
    
    for (size_t i = 2; i < lines.size(); i++) {
        if (lines[i].empty())
            continue;
                
        data = split(lines[i], " ");
        
        processes.emplace_back(
            data[NVSMI_NAME], data[NVSMI_TYPE],
            data[NVSMI_GPUIDX], data[NVSMI_PID],
            data[NVSMI_SM], data[NVSMI_MEM],
            data[NVSMI_ENC], data[NVSMI_DEC],
            data[NVSMI_FBMEM]
        );
    }
    
    mutex.unlock();
    
    dataUpdated();
}

int ProcessesWorker::processesIndexByPid(const std::string& pid) {
    for (size_t i = 0; i < processes.size(); i++)
        if (processes[i].pid == pid)
            return i;
        
    return -1;
}

ProcessesTableView::ProcessesTableView(QWidget *parent) : QTableView(parent) {
    worker = new ProcessesWorker;
    
    auto *model = new QStandardItemModel;
 
    // Column titles
    QStringList horizontalHeader;
    horizontalHeader.append("Name of Process"); // Longer header increases column width so the process names are visible
    horizontalHeader.append("Type (C/G)");
    horizontalHeader.append("GPU ID");
    horizontalHeader.append("Process ID");
    horizontalHeader.append("SM Util (%)");
    horizontalHeader.append("GPU Mem Util (%)");
    horizontalHeader.append("Encoding (%)");
    horizontalHeader.append("Decoding (%)");
    horizontalHeader.append("FB Mem Usage (MB)");
    
    model->setHorizontalHeaderLabels(horizontalHeader);
    
    setModel(model);
    resizeRowsToContents();
    resizeColumnsToContents();
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    verticalHeader()->hide();
    setAutoScroll(false);
}

ProcessesTableView::~ProcessesTableView() {
    delete worker;
}

void ProcessesTableView::mousePressEvent(QMouseEvent *event) {
    QTableView::mousePressEvent(event);
    int row = indexAt(event->pos()).row();
    
    if (row != -1) {
        QMutexLocker locker(&worker->mutex);
        setCurrentIndex(model()->index(row, 0));
        selectedPid = worker->processes[row].pid;
    } else
        selectedPid = "";
    
    if (event->button() == Qt::RightButton && row != -1) {
        QMenu contextMenu(tr("Context menu"), this);

        worker->mutex.lock();
        QAction action1(
            ("Kill " + worker->processes[row].name + " (pid " + worker->processes[row].pid + ")").c_str(),
            this
        );
        worker->mutex.unlock();
        connect(&action1, &QAction::triggered, this, &ProcessesTableView::killProcess);
        contextMenu.addAction(&action1);
    
        contextMenu.exec(mapToGlobal(event->pos()));
    }
}

void ProcessesTableView::killProcess() {
    QMutexLocker locker(&worker->mutex);
    if (worker->processesIndexByPid(selectedPid) != -1) {
        exec("kill " + selectedPid);
        selectedPid = "";
    }
}

void ProcessesTableView::_setItem(int row, int column, std::string str) {
    QStandardItem *qitem = new QStandardItem;

    char *end;
    long long i = strtoll( str.c_str(), &end, 10 ); // check if value can be converted to integer
    if ( *end == '\0' )  {
        qitem->setData(i, Qt::EditRole);
    } else {
        qitem->setData(str.c_str(), Qt::EditRole);
    }

    qitem->setTextAlignment(Qt::AlignHCenter); 
    ((QStandardItemModel*)model())->setItem(row, column, qitem);
}

void ProcessesTableView::onDataUpdated() {
    model()->removeRows(0, model()->rowCount());
    QMutexLocker locker(&worker->mutex);

    for (size_t i = 0; i < worker->processes.size(); i++) {
        _setItem(i, NVSM_NAME, worker->processes[i].name);
        _setItem(i, NVSM_TYPE, worker->processes[i].type);
        _setItem(i, NVSM_GPUIDX, worker->processes[i].gpuIdx);
        _setItem(i, NVSM_PID, worker->processes[i].pid);
        _setItem(i, NVSM_SM, worker->processes[i].sm);
        _setItem(i, NVSM_MEM, worker->processes[i].mem);
        _setItem(i, NVSM_ENC, worker->processes[i].enc);
        _setItem(i, NVSM_DEC, worker->processes[i].dec);
        _setItem(i, NVSM_FBMEM, worker->processes[i].fbmem);
    }

    setSortingEnabled(true);
    
    int index = worker->processesIndexByPid(selectedPid);
    if (index != -1)
        setCurrentIndex(model()->index(index, 0));
}
