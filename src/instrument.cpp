// Implementation of Instrument accept methods (Visitor Pattern)

#include "../include/instrument.hh"
#include "../include/visitor.hh"

void Stock::accept(InstrumentVisitor& visitor) {
    visitor.visit(*this);
}

void Stock::accept(ConstInstrumentVisitor& visitor) const {
    visitor.visit(*this);
}

void Option::accept(InstrumentVisitor& visitor) {
    visitor.visit(*this);
}

void Option::accept(ConstInstrumentVisitor& visitor) const {
    visitor.visit(*this);
}

void Bond::accept(InstrumentVisitor& visitor) {
    visitor.visit(*this);
}

void Bond::accept(ConstInstrumentVisitor& visitor) const {
    visitor.visit(*this);
}
