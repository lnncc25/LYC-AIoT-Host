#ifndef ICASE81RESULTPRESENTER_H
#define ICASE81RESULTPRESENTER_H

struct Case81Result;

class ICase81ResultPresenter
{
public:
    virtual ~ICase81ResultPresenter() = default;

    virtual void presentCase81(const Case81Result &result) = 0;
};

#endif
