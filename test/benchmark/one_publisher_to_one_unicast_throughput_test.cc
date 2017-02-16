// Copyright (c) 2011-2015, François Saint-Jacques
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the disruptor-- nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL FRANÇOIS SAINT-JACQUES BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#include <iostream>
#include <thread>

#include <disruptor/ring_buffer.h>
#include <disruptor/sequencer.h>

using namespace disruptor;

namespace
{
    const size_t ringBufferSize = 1024 * 8;
    const int64_t iterations = 1000L * 1000L * 300;
}

int main(int arc, char** argv)
{
    std::array<int64_t, ringBufferSize> buffer{};
    Sequencer<int64_t, ringBufferSize, SingleThreadedStrategy<ringBufferSize>, BusySpinStrategy> sequencer(buffer);

    auto barrier = sequencer.NewBarrier({});

    Sequence consumerSequence;
    sequencer.set_gating_sequences({ &consumerSequence });

    std::thread consumer([&barrier, &sequencer, &consumerSequence]
    {
        int64_t nextSequence = consumerSequence.sequence() + 1;
        while (nextSequence < iterations)
        {
            auto availableSequence = barrier.WaitFor(nextSequence);
            if (availableSequence >= disruptor::kInitialCursorValue)
            {
                nextSequence = availableSequence;
                consumerSequence.set_sequence(nextSequence);
            }
        }
    });

    auto start = std::chrono::steady_clock::now();

    for (int64_t i = 0; i < iterations; i++)
    {
        auto sequence = sequencer.Claim(1);
        sequencer[sequence] = i;
        sequencer.Publish(sequence);
    }

    int64_t expected_sequence = sequencer.GetCursor();
    while (consumerSequence.sequence() < expected_sequence) {}

    auto end = std::chrono::steady_clock::now();

    std::cout.precision(15);
    std::cout << "1P-1EP-UNICAST performance: ";
    std::cout << iterations / std::chrono::duration<double, std::ratio<1>>(end - start).count() << " ops/secs" << std::endl;

    consumer.join();

    return EXIT_SUCCESS;
}

