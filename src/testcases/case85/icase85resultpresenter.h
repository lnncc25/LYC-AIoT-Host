#ifndef ICASE85RESULTPRESENTER_H
#define ICASE85RESULTPRESENTER_H

#include "case85model.h"

class ICase85ResultPresenter
{
public:
    virtual ~ICase85ResultPresenter() = default;
    virtual void presentCase85Initial(int totalRows) = 0;
    virtual void presentCase85Row(int row, const Case85RowResult &result) = 0;
    virtual void presentCase85Result(const Case85Result &result) = 0;
};

#endif
