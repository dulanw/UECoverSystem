# UECoverSystem
UE4 c++ asynchronous cover system generator and sample EQS template to selecting cover, a sample Env Query Generator is included with Env Query Test which selects a cover point that makes sure that the cover point is valid against the primary target context (needs to provide your own, which can be a simple context that reads a target blackboard key) and scores the valid points (if multiple are valid) against the rest of the enemies, the pawn may implement the interface provided which needs to return a list of enemy actors.

Most of the code is from https://github.com/GlassBeaver/CoverSystem but improvments have been made to fix some crashes, allow more flexibility and improved cover selection, for example, it will the cover test selects an cover point that is directly in view of the primary enemy (discard cover point if there are seconday obstacles other than the actual cover itself)

#TODO add instructions for setting up the EQS.
