from . import board


def last_human_comparison(state):
    for index in range(len(state["moves"]) - 1, -1, -1):
        move = state["moves"][index]
        if move["symbol"] == state["human"]:
            before = board.replay(state["moves"][:index])
            return {"human_move_number": index + 1,
                    **board.compare_move(before, move["column"], state["human"])}
    return None


def evidence_for(state, kind):
    if kind == "review":
        comparisons = []
        position = board.EMPTY
        for index, move in enumerate(state["moves"]):
            if move["symbol"] == state["human"]:
                comparisons.append({"human_move_number": index + 1,
                                    **board.compare_move(position, move["column"], move["symbol"])})
            position = board.drop(position, move["column"], move["symbol"])
        return {"human_moves": comparisons}, None
    comparison = last_human_comparison(state)
    if kind == "feedback":
        return {"last_human_move": comparison}, comparison["best_column"] if comparison else None
    tactical = board.analyse(tuple(state["board"]), state["human"])
    return {"current": tactical, "last_human_move": comparison}, tactical["recommended_column"]


def feedback_text(comparison):
    if comparison["won"]:
        return "You spotted the winning move."
    if comparison["missed_win"]:
        return f"You had an immediate win in column {comparison['best_column']}."
    if comparison["allowed_immediate_win"]:
        return "That move left an immediate winning reply for the robot."
    if comparison["created_fork"]:
        return "You created two immediate winning threats, a fork."
    if comparison["score_loss"] == 0:
        return "Your move matched a top choice in the five-ply search."
    return f"The five-ply search preferred column {comparison['best_column']} in that position."


def fallback(evidence, kind):
    if kind == "review":
        comparisons = evidence["human_moves"]
        good = next((c for c in comparisons if c["won"] or c["created_fork"] or c["score_loss"] == 0), None)
        improvements = sorted((c for c in comparisons if c["score_loss"] > 0),
                              key=lambda c: c["score_loss"], reverse=True)[:2]
        parts = [f"At move {good['human_move_number']}, {feedback_text(good)}"] if good else [
            "Keep looking for immediate wins and threats before choosing a column."]
        parts += [f"At move {c['human_move_number']}, {feedback_text(c)}" for c in improvements]
        if not improvements:
            parts.append("This search found no higher-scoring alternatives to your moves.")
        return " ".join(parts)
    comparison = evidence.get("last_human_move")
    if kind == "feedback":
        return feedback_text(comparison) if comparison else "Look for immediate wins and threats."
    current = evidence["current"]
    if current["immediate_wins"]:
        hint = f"You can win now in column {current['immediate_wins'][0]}."
    elif current["opponent_wins"]:
        hint = f"The robot has an immediate threat; consider column {current['recommended_column']}."
    elif current["fork_columns"]:
        hint = f"Column {current['fork_columns'][0]} creates two immediate winning threats."
    else:
        hint = f"Consider column {current['recommended_column']}; centre control and future threats matter."
    return (feedback_text(comparison) + " " if comparison else "") + hint
