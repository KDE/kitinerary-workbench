# KItinerary Workbench

Interactive test and inspection tool for the reservation data extraction.

## Usage

KItinerary Workbench is structured into to main UI parts, the input panel on the left,
and the output panel on the right.

The input panel allows specifying the input data and its context, as well as inspecting
pre-processed input data (such as textual and image data extracted from a PDF). The output
panel allows to inspect the result of the various extractor and post-processing stages.

To test an extractor, specify the input data on the source tab of the input panel, either
by opening a file (via the file open dialgo, or e.g. by dnd-ing an email attachment on to the
file input line), or by entering textual source data directly in the text field. If not detected
automatically, you also need to set the right input data type (plain text, HTML, PDF, Apple Wallet
passes, IATA boarding pass codes, UIC 918.3 train ticket codes, etc).

For structured data extractors this should already show results in the output panel then, for
unstructured data extractors you additionally need to specify the sender email (used to pick
the right extractor script) and optionally a context date (used to resolve date/time ambiguities).

## Extractor Development

TODO
