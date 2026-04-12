for (step = 0; step < steps; step++) {
    for (each cell) {
        compute neighbors from state
        write result to next_state
    }
    LB_swap(state, next_state);
}
