import Foundation
import Security
import CryptoKit

struct Enrollment: Codable {
    let endpoint: URL
    let fingerprint: String
    let name: String
    let id: String
    let token: String
}
enum EnrollmentStore {
    private static let query: [String: Any] = [kSecClass as String: kSecClassGenericPassword, kSecAttrService as String: "camera.camber.enrollment"]
    static func save(_ enrollment: Enrollment) throws {
        let data = try JSONEncoder().encode(enrollment)
        SecItemDelete(query as CFDictionary)
        var item = query
        item[kSecValueData as String] = data
        item[kSecAttrAccessible as String] = kSecAttrAccessibleWhenUnlockedThisDeviceOnly
        guard SecItemAdd(item as CFDictionary, nil) == errSecSuccess else { throw CamberError.message("Could not protect pairing credentials.") }
    }
    static func load() -> Enrollment? {
        var item = query; item[kSecReturnData as String] = true
        var result: CFTypeRef?
        guard SecItemCopyMatching(item as CFDictionary, &result) == errSecSuccess, let data = result as? Data else { return nil }
        return try? JSONDecoder().decode(Enrollment.self, from: data)
    }
    static func remove() { SecItemDelete(query as CFDictionary) }
}
enum CamberError: LocalizedError {
    case message(String)
    var errorDescription: String? { if case .message(let value) = self { return value }; return nil }
}
final class PinnedSession: NSObject, URLSessionDelegate {
    let fingerprint: String
    lazy var session = URLSession(configuration: .ephemeral, delegate: self, delegateQueue: nil)
    init(fingerprint: String) { self.fingerprint = fingerprint.uppercased() }
    func urlSession(_ session: URLSession, didReceive challenge: URLAuthenticationChallenge,
                    completionHandler: @escaping (URLSession.AuthChallengeDisposition, URLCredential?) -> Void) {
        guard challenge.protectionSpace.authenticationMethod == NSURLAuthenticationMethodServerTrust,
              let trust = challenge.protectionSpace.serverTrust, let certificate = SecTrustGetCertificateAtIndex(trust, 0) else {
            completionHandler(.cancelAuthenticationChallenge, nil); return
        }
        let digest = SHA256.hash(data: SecCertificateCopyData(certificate) as Data).map { String(format: "%02X", $0) }.joined()
        if digest == fingerprint { completionHandler(.useCredential, URLCredential(trust: trust)) }
        else { completionHandler(.cancelAuthenticationChallenge, nil) }
    }
}
