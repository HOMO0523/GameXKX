"""Stop the temporary VFX recording loop without leaving PIE."""
import builtins
state = getattr(builtins, '_lightning057_preview', None)
if state:
    state['stop']()
print('Lightning preview stopped; PIE remains open.')
