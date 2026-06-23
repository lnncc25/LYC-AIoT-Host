#ifndef ICASE82RESULTPRESENTER_H
#define ICASE82RESULTPRESENTER_H

#include "case82model.h"

class ICase82ResultPresenter
{
public:
    virtual ~ICase82ResultPresenter() = default;
    virtual void presentCase82Initial(const Case82RunConfig &config) = 0;
    virtual void presentCase82Sample(const Case82Sample &sample) = 0;
    virtual void presentCase82Row(int row, const Case82RowResult &result) = 0;
    virtual void presentCase82Result(const Case82Result &result) = 0;
};

#endif
