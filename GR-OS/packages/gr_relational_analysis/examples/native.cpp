#include "../tests/fixture.hpp"
#include <iostream>
int main() {
    analysis_fixture::Fixture fixture;
    fixture.small();
    auto incidences = fixture.incidence(fixture.selected());
    auto network = gr_relational_analysis::analyze(incidences, fixture.session);
    std::cout << gr_relational_analysis::summarize(network).dump(2) << '\n';
    return 0;
}
