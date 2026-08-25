/** @vitest-environment jsdom */
import { fireEvent, render, waitFor } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import Home from "./Home";
import { requestCppBattleSimulation } from "../lib/cppSnapshot";

vi.mock("../lib/cppSnapshot", () => ({
  requestCppBattleSimulation: vi.fn(),
}));

describe("Home battle mode", () => {
  it("starts with long battle enabled and forwards the disabled mode to C++", async () => {
    const simulate = vi.mocked(requestCppBattleSimulation);
    simulate.mockResolvedValue({ rawText: "甲\n\n乙", initialPlayers: [], finalPlayers: [], commands: [], utf8ByteLength: 0, transportHash: 1, winnerTeamId: 1, momentCount: 0, executedActionCount: 0, longBattle: false });
    const { getByRole, getByLabelText } = render(<Home />);
    const toggle = getByRole("button", { name: /长对局/ });

    expect(toggle.getAttribute("aria-pressed")).toBe("true");
    fireEvent.click(toggle);
    expect(toggle.getAttribute("aria-pressed")).toBe("false");
    fireEvent.change(getByLabelText("输入名字"), { target: { value: "甲\n\n乙" } });
    fireEvent.click(getByRole("button", { name: "开始对战" }));

    await waitFor(() => expect(simulate).toHaveBeenCalledWith("甲\n\n乙", false));
  });
});
